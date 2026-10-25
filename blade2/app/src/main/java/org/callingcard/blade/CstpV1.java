package org.callingcard.blade;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.GeneralSecurityException;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.Mac;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** CSTP v1 pure cryptographic/codec utility. Not wired into BLE telemetry. */
public final class CstpV1 {
    public static final int C2B = 1, B2C = 2, HEADER_SIZE = 21, MAX_PAYLOAD = 256;
    private static final byte[] PREFIX = utf8("CSTP-v1");
    private CstpV1() {}
    private static byte[] utf8(String s) { return s.getBytes(StandardCharsets.US_ASCII); }
    private static byte[] append(byte[]... parts) {
        int n=0; for(byte[] p:parts) n+=p.length;
        byte[] out=new byte[n]; int pos=0;
        for(byte[] p:parts){System.arraycopy(p,0,out,pos,p.length);pos+=p.length;}
        return out;
    }
    private static byte[] label(String s) { return append(utf8(s), new byte[]{0}); }
    private static void exact(byte[] b,int n,String name) {
        if(b==null || b.length!=n)throw new IllegalArgumentException(name);
    }
    private static byte[] sha256(byte[] data) throws GeneralSecurityException {
        return MessageDigest.getInstance("SHA-256").digest(data);
    }
    private static byte[] hmac(byte[] key,byte[] data) throws GeneralSecurityException {
        Mac mac=Mac.getInstance("HmacSHA256");
        mac.init(new SecretKeySpec(key,"HmacSHA256"));
        return mac.doFinal(data);
    }
    public static byte[] transcript(byte[] card,byte[] blade) {
        exact(card,32,"Cardputer challenge"); exact(blade,32,"Blade challenge");
        boolean c=false,b=false;
        for(byte x:card)c |= x!=0;
        for(byte x:blade)b |= x!=0;
        if(!c || !b)throw new IllegalArgumentException("zero challenge");
        return append(label("CSTP-v1"),card,blade);
    }
    public static byte[] sessionId(byte[] transcript) throws GeneralSecurityException {
        return Arrays.copyOf(sha256(append(label("CSTP-v1 sid"),transcript)),8);
    }
    public static byte[] proof(byte[] psk,byte[] t,boolean blade) throws GeneralSecurityException {
        exact(psk,32,"PSK");
        return hmac(psk,append(label(blade?"CSTP-v1 blade-proof":"CSTP-v1 cardputer-proof"),t));
    }
    public static boolean verifyProof(byte[] psk,byte[] t,boolean blade,byte[] received)
            throws GeneralSecurityException {
        return received!=null && MessageDigest.isEqual(proof(psk,t,blade),received);
    }
    private static byte[] hkdfExpand(byte[] prk,byte[] info,int size) throws GeneralSecurityException {
        if(size < 1 || size > 255*32)throw new IllegalArgumentException("HKDF length");
        byte[] out=new byte[size],prev=new byte[0]; int used=0;
        for(int counter=1;used<size;counter++){
            prev=hmac(prk,append(prev,info,new byte[]{(byte)counter}));
            int n=Math.min(prev.length,size-used);
            System.arraycopy(prev,0,out,used,n); used+=n;
        }
        return out;
    }
    public static final class Secrets {
        public final byte[] key,noncePrefix;
        private Secrets(byte[] k,byte[] p){key=k;noncePrefix=p;}
    }
    public static Secrets derive(byte[] psk,byte[] t,int direction)
            throws GeneralSecurityException {
        exact(psk,32,"PSK");
        if(direction!=C2B && direction!=B2C)throw new IllegalArgumentException("direction");
        String d=direction==C2B?"c2b":"b2c";
        byte[] sid=sessionId(t);
        byte[] salt=sha256(append(label("CSTP-v1 salt"),t));
        byte[] prk=hmac(salt,psk);
        return new Secrets(
          hkdfExpand(prk,append(label("CSTP-v1 key "+d),sid),32),
          hkdfExpand(prk,append(label("CSTP-v1 nonce "+d),sid),4));
    }
    private static byte[] header(int direction,int kind,byte[] sid,long seq,int len){
        exact(sid,8,"sid");
        if(direction!=C2B && direction!=B2C)throw new IllegalArgumentException("direction");
        if(kind!=0x10 && kind!=0x11 && kind!=0x12 && kind!=0x20)throw new IllegalArgumentException("kind");
        if(len<0 || len>MAX_PAYLOAD)throw new IllegalArgumentException("length");
        // Java signed long: reject values in upper half of unsigned 64-bit domain.
        if(seq<0)throw new IllegalArgumentException("sequence");
        return ByteBuffer.allocate(HEADER_SIZE).order(ByteOrder.BIG_ENDIAN)
          .put((byte)1).put((byte)direction).put((byte)kind).put(sid).putLong(seq).putShort((short)len).array();
    }
    private static byte[] nonce(Secrets s,long seq){
        exact(s.noncePrefix,4,"nonce prefix");
        return append(s.noncePrefix,ByteBuffer.allocate(8).putLong(seq).array());
    }
    public static byte[] seal(Secrets s,int direction,int kind,byte[] sid,long seq,byte[] plain)
            throws GeneralSecurityException {
        if(plain==null)throw new IllegalArgumentException("plaintext");
        byte[] hdr=header(direction,kind,sid,seq,plain.length);
        exact(s.key,32,"key");
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE,new SecretKeySpec(s.key,"AES"),new GCMParameterSpec(128,nonce(s,seq)));
        cipher.updateAAD(hdr);
        return append(hdr,cipher.doFinal(plain));
    }
    public static byte[] open(Secrets s,int expectedDirection,byte[] sid,long expectedSeq,byte[] frame)
            throws GeneralSecurityException {
        if(frame==null || frame.length<HEADER_SIZE+16 || frame.length>HEADER_SIZE+MAX_PAYLOAD+16)
            throw new IllegalArgumentException("frame size");
        ByteBuffer b=ByteBuffer.wrap(frame).order(ByteOrder.BIG_ENDIAN);
        int version=b.get()&255, direction=b.get()&255, kind=b.get()&255;
        byte[] actualSid=new byte[8];b.get(actualSid);
        long seq=b.getLong();int len=b.getShort()&65535;
        if(version!=1 || direction!=expectedDirection || !MessageDigest.isEqual(sid,actualSid)
            || seq!=expectedSeq || seq<0 || frame.length!=HEADER_SIZE+len+16)
            throw new IllegalArgumentException("frame header/replay");
        header(direction,kind,actualSid,seq,len);
        exact(s.key,32,"key");
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.DECRYPT_MODE,new SecretKeySpec(s.key,"AES"),new GCMParameterSpec(128,nonce(s,seq)));
        cipher.updateAAD(Arrays.copyOf(frame,HEADER_SIZE));
        return cipher.doFinal(Arrays.copyOfRange(frame,HEADER_SIZE,frame.length));
    }
}
