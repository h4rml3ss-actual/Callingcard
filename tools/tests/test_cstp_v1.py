"""CSTP reference tests. Run: python -m unittest discover -s tools/tests -v"""
import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from cryptography.exceptions import InvalidTag
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
import cstp_v1 as c


class CSTPTests(unittest.TestCase):
    psk = bytes(range(32))
    challenge_c = bytes(range(1, 33))
    challenge_b = bytes(range(33, 65))

    def session(self):
        t = c.transcript(self.challenge_c, self.challenge_b)
        return t, c.session_id(t), c.derive(self.psk, t, c.C2B)

    def test_nist_aes256_gcm_empty_plaintext_known_answer(self):
        # NIST SP 800-38D zero key/IV, empty plaintext and AAD.
        actual = AESGCM(bytes(32)).encrypt(bytes(12), b"", b"")
        self.assertEqual(actual.hex(), "530f8afbc74536b9a963b4f1c4cb738b")

    def test_transcript_and_proof(self):
        t, sid, sec = self.session()
        self.assertEqual(t[:8], b"CSTP-v1\x00")
        self.assertEqual(len(sid), 8)
        self.assertNotEqual(c.proof(self.psk, t, "blade"), c.proof(self.psk, t, "cardputer"))
        self.assertTrue(c.verify_proof(self.psk, t, "blade", c.proof(self.psk, t, "blade")))
        self.assertFalse(c.verify_proof(bytes(32), t, "blade", c.proof(self.psk, t, "blade")))
        self.assertNotEqual(sec, c.derive(self.psk, t, c.B2C))

    def test_round_trip_and_wrong_session(self):
        _, sid, sec = self.session()
        frame = c.seal(sec, c.C2B, 0x10, sid, 0, b"v1,1,2,3,0,1")
        self.assertEqual(len(frame), c.HEADER_LENGTH + 12 + 16)
        self.assertEqual(c.open_frame(sec, c.C2B, sid, 0, frame), (0x10, b"v1,1,2,3,0,1"))
        with self.assertRaises(ValueError):
            c.open_frame(sec, c.B2C, sid, 0, frame)
        with self.assertRaises(ValueError):
            c.open_frame(sec, c.C2B, bytes(8), 0, frame)

    def test_replay_and_tampering(self):
        _, sid, sec = self.session()
        frame = c.seal(sec, c.C2B, 0x11, sid, 0, b"alert")
        with self.assertRaises(ValueError):
            c.open_frame(sec, c.C2B, sid, 1, frame)
        altered = bytearray(frame)
        altered[-1] ^= 1
        with self.assertRaises(InvalidTag):
            c.open_frame(sec, c.C2B, sid, 0, bytes(altered))
        altered = bytearray(frame)
        altered[2] = 0x10   # a valid type with modified AAD
        with self.assertRaises(InvalidTag):
            c.open_frame(sec, c.C2B, sid, 0, bytes(altered))
        with self.assertRaises(ValueError):
            c.open_frame(sec, c.C2B, sid, 0, frame[:-1])

    def test_boundary_and_malformed(self):
        with self.assertRaises(ValueError):
            c.transcript(bytes(32), self.challenge_b)
        with self.assertRaises(ValueError):
            c.parse_control(b"\x01\x02" + bytes(32))
        self.assertEqual(c.parse_control(b"\x01\x01" + self.challenge_c)[0], 1)
        _, sid, sec = self.session()
        with self.assertRaises(ValueError):
            c.seal(sec, c.C2B, 0x10, sid, 0, bytes(c.MAX_PAYLOAD + 1))
        with self.assertRaises(ValueError):
            c.encode_header(c.C2B, 0x10, sid, -1, 0)
        with self.assertRaises(ValueError):
            c.encode_header(c.C2B, 0x10, sid, 1 << 64, 0)

if __name__ == "__main__":
    unittest.main()
