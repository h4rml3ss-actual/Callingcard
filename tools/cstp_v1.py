"""CSTP v1 reference codec and cryptographic derivations (host test utility ONLY).

Requires Python 3.10+ and cryptography. Never use a test key in production.
Wire contract: docs/CSTP_V1_SECURITY_PROTOCOL.md.
"""
from __future__ import annotations
import hashlib
import hmac
import struct
from dataclasses import dataclass
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.hkdf import HKDFExpand

VERSION = 1
C2B, B2C = 1, 2
HEADER_LENGTH = 21
MAX_PAYLOAD = 256
PREFIX = b"CSTP-v1"
MAX_SEQUENCE = (1 << 64) - 1


def check_bytes(value: bytes, size: int, label: str) -> None:
    if not isinstance(value, bytes) or len(value) != size:
        raise ValueError(f"{label} must be exactly {size} bytes")


def transcript(cardputer_challenge: bytes, blade_challenge: bytes) -> bytes:
    check_bytes(cardputer_challenge, 32, "Cardputer challenge")
    check_bytes(blade_challenge, 32, "Blade challenge")
    if not any(cardputer_challenge) or not any(blade_challenge):
        raise ValueError("zero challenge is invalid")
    return PREFIX + b"\x00" + cardputer_challenge + blade_challenge


def session_id(t: bytes) -> bytes:
    return hashlib.sha256(b"CSTP-v1 sid\x00" + t).digest()[:8]


def proof(psk: bytes, t: bytes, role: str) -> bytes:
    check_bytes(psk, 32, "root PSK")
    if role not in ("blade", "cardputer"):
        raise ValueError("invalid role")
    return hmac.digest(psk, b"CSTP-v1 " + role.encode("ascii") + b"-proof\x00" + t, "sha256")


def verify_proof(psk: bytes, t: bytes, role: str, received: bytes) -> bool:
    return isinstance(received, bytes) and hmac.compare_digest(proof(psk, t, role), received)


@dataclass(frozen=True)
class DirectionalSecrets:
    key: bytes
    prefix: bytes


def derive(psk: bytes, t: bytes, direction: int) -> DirectionalSecrets:
    check_bytes(psk, 32, "root PSK")
    if direction not in (C2B, B2C):
        raise ValueError("invalid direction")
    suffix = b"c2b" if direction == C2B else b"b2c"
    salt = hashlib.sha256(b"CSTP-v1 salt\x00" + t).digest()
    prk = hmac.digest(salt, psk, "sha256")  # HKDF-Extract
    sid = session_id(t)
    def expand(label: bytes, n: int) -> bytes:
        return HKDFExpand(algorithm=__import__("cryptography.hazmat.primitives.hashes", fromlist=["SHA256"]).SHA256(), length=n, info=label + b"\x00" + sid).derive(prk)
    return DirectionalSecrets(expand(b"CSTP-v1 key " + suffix, 32),
                              expand(b"CSTP-v1 nonce " + suffix, 4))


def encode_header(direction: int, kind: int, sid: bytes, sequence: int, length: int) -> bytes:
    check_bytes(sid, 8, "session id")
    if direction not in (C2B, B2C) or kind not in (0x10, 0x11, 0x12, 0x20):
        raise ValueError("invalid direction or message type")
    if not 0 <= sequence <= MAX_SEQUENCE or not 0 <= length <= MAX_PAYLOAD:
        raise ValueError("invalid sequence or length")
    return struct.pack(">BBB8sQH", VERSION, direction, kind, sid, sequence, length)


def seal(secrets: DirectionalSecrets, direction: int, kind: int,
         sid: bytes, sequence: int, plaintext: bytes) -> bytes:
    if not isinstance(plaintext, bytes):
        raise ValueError("payload must be bytes")
    check_bytes(secrets.key, 32, "AES key")
    check_bytes(secrets.prefix, 4, "nonce prefix")
    header = encode_header(direction, kind, sid, sequence, len(plaintext))
    nonce = secrets.prefix + sequence.to_bytes(8, "big")
    return header + AESGCM(secrets.key).encrypt(nonce, plaintext, header)


def open_frame(secrets: DirectionalSecrets, expected_direction: int, sid: bytes,
               next_sequence: int, frame: bytes) -> tuple[int, bytes]:
    if len(frame) < HEADER_LENGTH + 16 or len(frame) > HEADER_LENGTH + MAX_PAYLOAD + 16:
        raise ValueError("invalid frame size")
    version, direction, kind, frame_sid, sequence, length = struct.unpack(
        ">BBB8sQH", frame[:HEADER_LENGTH])
    if version != VERSION or direction != expected_direction or frame_sid != sid:
        raise ValueError("version, direction or session mismatch")
    if sequence != next_sequence:
        raise ValueError("replay or out-of-order frame")
    if length > MAX_PAYLOAD or len(frame) != HEADER_LENGTH + length + 16:
        raise ValueError("invalid ciphertext length")
    encode_header(direction, kind, sid, sequence, length)  # reject unknown type
    check_bytes(secrets.key, 32, "AES key")
    check_bytes(secrets.prefix, 4, "nonce prefix")
    nonce = secrets.prefix + sequence.to_bytes(8, "big")
    return kind, AESGCM(secrets.key).decrypt(
        nonce, frame[HEADER_LENGTH:], frame[:HEADER_LENGTH])


def parse_control(message: bytes) -> tuple[int, bytes]:
    if not isinstance(message, bytes) or len(message) < 2 or message[1] != VERSION:
        raise ValueError("bad control message")
    kind = message[0]
    if (kind == 1 and len(message) == 34 or
        kind == 2 and len(message) == 66 or
        kind == 3 and len(message) == 34 or
        kind == 0x7f and len(message) == 3):
        return kind, message[2:]
    raise ValueError("invalid control message shape")
