#!/usr/bin/env python3
"""
LXMF Compatibility Test Suite for Ratdeck.

Two test layers:
  1. RAW PACKET tests — send RNS packets directly, verify via serial. Tests firmware.
  2. LXMF ROUTER tests — use Python LXMRouter, tests end-to-end client compatibility.

Usage:
  python3 test_lxmf_compat.py                              # discover mode (LoRa)
  python3 test_lxmf_compat.py <hash>                       # full suite (LoRa)
  python3 test_lxmf_compat.py <hash> --tcp                  # full suite (TCP)
  python3 test_lxmf_compat.py <hash> --suite raw            # raw packet tests only
  python3 test_lxmf_compat.py <hash> --suite lxmf           # LXMF router tests only
  python3 test_lxmf_compat.py <hash> --suite listen          # listen for Ratdeck msgs
  python3 test_lxmf_compat.py <hash> --suite all             # everything (default)
"""
import RNS
import LXMF
import time
import sys
import os
import shutil
import struct
import hashlib
import argparse

# --- Config ---
LORA_CONFIG = {
    "port": "/dev/cu.usbserial-0001",
    "frequency": 915000000,
    "bandwidth": 250000,
    "txpower": 14,
    "spreadingfactor": 7,
    "codingrate": 5,
    "preamble": 18,
}
TCP_HOST = "3.ratspeak.org"
TCP_PORT = 4343
STORAGE_PATH = "/tmp/ratdeck_test_storage"
IDENTITY_PATH = "/tmp/ratdeck_test_identity"

# --- State ---
results = {}
messages_rx = []


def decode_name(app_data):
    if not app_data:
        return ""
    try:
        d = app_data.decode("utf-8")
        if d.isprintable():
            return d
    except:
        pass
    if len(app_data) >= 3 and app_data[0] == 0x91 and app_data[1] == 0xC4:
        n = app_data[2]
        if len(app_data) >= 3 + n:
            try:
                return app_data[3:3 + n].decode("utf-8")
            except:
                pass
    return ""


class AnnounceHandler:
    aspect_filter = "lxmf.delivery"
    def received_announce(self, destination_hash, announced_identity, app_data):
        name = decode_name(app_data)
        if name:
            print(f"  [ANNOUNCE] {destination_hash.hex()} \"{name}\"", flush=True)


def on_rx(message):
    content = message.content.decode("utf-8") if message.content else ""
    sender = message.source_hash.hex()[:16]
    messages_rx.append({"content": content, "sender": sender, "time": time.time()})
    print(f"  [RX] from {sender}...: \"{content[:80]}\"", flush=True)


def init_rns(transport="lora"):
    os.makedirs(STORAGE_PATH, exist_ok=True)
    config_path = os.path.join(STORAGE_PATH, "config")

    if transport == "lora":
        lc = LORA_CONFIG
        iface_config = f"""  [[RNode LoRa]]
    type = RNodeInterface
    enabled = true
    port = {lc['port']}
    frequency = {lc['frequency']}
    bandwidth = {lc['bandwidth']}
    txpower = {lc['txpower']}
    spreadingfactor = {lc['spreadingfactor']}
    codingrate = {lc['codingrate']}
    preamble = {lc['preamble']}
    flow_control = false"""
    else:
        iface_config = f"""  [[TCP Hub]]
    type = TCPClientInterface
    enabled = true
    target_host = {TCP_HOST}
    target_port = {TCP_PORT}
    kiss_framing = false"""

    with open(config_path, "w") as f:
        f.write(f"""[reticulum]
  enable_transport = false
  share_instance = false
  shared_instance_port = 37433
  instance_control_port = 37434
  panic_on_interface_errors = no

[interfaces]
  [[Default Interface]]
    type = AutoInterface
    enabled = false

{iface_config}
""")

    r = RNS.Reticulum(configdir=STORAGE_PATH, loglevel=RNS.LOG_DEBUG)

    if os.path.exists(IDENTITY_PATH):
        identity = RNS.Identity.from_file(IDENTITY_PATH)
    else:
        identity = RNS.Identity()
        identity.to_file(IDENTITY_PATH)

    router = LXMF.LXMRouter(identity=identity, storagepath=STORAGE_PATH)
    dd = router.register_delivery_identity(identity, display_name="RatdeckTest")
    router.register_delivery_callback(on_rx)
    RNS.Transport.register_announce_handler(AnnounceHandler())
    router.announce(dd.hash)

    print(f"[INIT] transport={transport} identity={identity.hash.hex()[:16]}...")
    print(f"[INIT] LXMF dest={dd.hash.hex()}", flush=True)
    return router, dd, identity


def wait_for_path(dest_hash, timeout=90):
    print(f"[PATH] Waiting for target (up to {timeout}s)...", flush=True)
    for i in range(timeout // 3):
        has_id = RNS.Identity.recall(dest_hash) is not None
        has_path = RNS.Transport.has_path(dest_hash)
        if has_id and has_path:
            hops = RNS.Transport.hops_to(dest_hash)
            print(f"[PATH] Ready after {(i+1)*3}s (hops={hops})", flush=True)
            return True
        if i % 5 == 0:
            RNS.Transport.request_path(dest_hash)
        time.sleep(3)
    print("[PATH] TIMEOUT", flush=True)
    return False


# ============================================================
# RAW PACKET HELPERS — bypass Python LXMF router entirely
# ============================================================

def build_lxmf_packet(identity, dest_hash, content_str):
    """Build a raw LXMF opportunistic payload."""
    content_bytes = content_str.encode("utf-8")
    src_hash = identity.hash[:16]
    timestamp = time.time()
    # MsgPack: fixarray(4) [float64, bin8(title), bin8(content), fixmap(0)]
    packed = (bytes([0x94, 0xCB]) + struct.pack(">d", timestamp) +
              bytes([0xC4, 0]) +  # empty title
              bytes([0xC4, len(content_bytes)]) + content_bytes +
              bytes([0x80]))  # empty fields
    signable_prefix = dest_hash[:16] + src_hash
    hash_input = signable_prefix + packed
    msg_hash = hashlib.sha256(hash_input).digest()
    signature = identity.sign(hash_input + msg_hash)
    return src_hash + signature + packed


def send_raw_and_wait(identity, dest, dest_hash, content, label, wait_s=15):
    """Send a raw LXMF packet and wait for proof. Returns (delivered, elapsed)."""
    payload = build_lxmf_packet(identity, dest_hash, content)
    packet = RNS.Packet(dest, payload)
    start = time.time()
    receipt = packet.send()

    for _ in range(wait_s * 4):
        time.sleep(0.25)
        if receipt.status == RNS.PacketReceipt.DELIVERED:
            elapsed = time.time() - start
            results[label] = "DELIVERED"
            return True, elapsed
        elif receipt.status == RNS.PacketReceipt.FAILED:
            results[label] = "FAILED"
            return False, time.time() - start

    results[label] = "TIMEOUT"
    return False, time.time() - start


# ============================================================
# RAW PACKET TEST SUITES — firmware verification
# ============================================================

def run_raw_size_tests(identity, dest, dest_hash):
    """Test different content sizes via raw packets."""
    print("\n" + "=" * 60)
    print("RAW PACKET: SIZE TESTS")
    print("=" * 60)

    sizes = [
        ("RS1", 10,   "minimal"),
        ("RS2", 50,   "short"),
        ("RS3", 100,  "typical"),
        ("RS4", 150,  "medium"),
        ("RS5", 200,  "long"),
        ("RS6", 250,  "near frame limit"),
    ]

    passed = 0
    for label, size, desc in sizes:
        content = f"{label}:" + "X" * max(0, size - len(label) - 1)
        ok, elapsed = send_raw_and_wait(identity, dest, dest_hash, content, label)
        icon = "OK" if ok else "FAIL"
        if ok:
            passed += 1
        print(f"  [{icon}] {label}: {size}B ({desc}) -> {results[label]} in {elapsed:.1f}s", flush=True)
        time.sleep(3)

    print(f"\n  Raw size: {passed}/{len(sizes)} passed")


def run_raw_unicode_tests(identity, dest, dest_hash):
    """Test Unicode content via raw packets."""
    print("\n" + "=" * 60)
    print("RAW PACKET: UNICODE TESTS")
    print("=" * 60)

    msgs = [
        ("RU1", "Hello ASCII baseline",              "ASCII"),
        ("RU2", "Привет из теста!",                    "Cyrillic"),
        ("RU3", "你好世界 テスト 🌍",                  "CJK + emoji"),
        ("RU4", "🔐📡🛰️ mesh crypto LoRa",           "Multi-emoji"),
        ("RU5", "Ñoño café résumé naïve über Zürich", "Latin Extended"),
        ("RU6", "مرحبا العالم",                         "Arabic RTL"),
    ]

    passed = 0
    for label, content, desc in msgs:
        ok, elapsed = send_raw_and_wait(identity, dest, dest_hash, content, label)
        icon = "OK" if ok else "FAIL"
        if ok:
            passed += 1
        print(f"  [{icon}] {label}: \"{content[:35]}\" ({desc}) -> {elapsed:.1f}s", flush=True)
        time.sleep(3)

    print(f"\n  Raw unicode: {passed}/{len(msgs)} passed")


def run_raw_timing_tests(identity, dest, dest_hash):
    """Test rapid sending via raw packets."""
    print("\n" + "=" * 60)
    print("RAW PACKET: TIMING TESTS")
    print("=" * 60)

    intervals = [
        ("RT1", 5.0, 5,  "5s cadence"),
        ("RT2", 2.0, 5,  "2s cadence"),
        ("RT3", 1.0, 5,  "1s cadence"),
        ("RT4", 0.5, 5,  "500ms cadence"),
        ("RT5", 2.0, 10, "sustained 10 msgs"),
    ]

    for group_label, interval, count, desc in intervals:
        print(f"\n--- {group_label}: {count} msgs, {interval}s apart ({desc}) ---")
        passed = 0
        for i in range(count):
            label = f"{group_label}-{i+1}"
            content = f"{group_label} msg {i+1}/{count}"
            ok, elapsed = send_raw_and_wait(identity, dest, dest_hash, content, label, wait_s=10)
            if ok:
                passed += 1
            else:
                print(f"  [FAIL] {label}: {results[label]} ({elapsed:.1f}s)", flush=True)
            time.sleep(interval)
        print(f"  {group_label}: {passed}/{count} delivered", flush=True)


# ============================================================
# LXMF ROUTER TEST SUITES — Python client compatibility
# ============================================================

def run_lxmf_link_test(router, dest, source):
    """Test link-based delivery via LXMRouter."""
    print("\n" + "=" * 60)
    print("LXMF ROUTER: LINK TESTS (DIRECT method)")
    print("=" * 60)

    print("\n--- LK1: Single message via link (60s timeout) ---")
    msg = LXMF.LXMessage(dest, source, "LK1: Link delivery test")
    msg.desired_method = LXMF.LXMessage.DIRECT
    def cb(m):
        results["LK1"] = f"s{m.state}"
        print(f"  [LK1] -> s{m.state}", flush=True)
    msg.register_delivery_callback(cb)
    router.handle_outbound(msg)
    print("  Queued. Waiting 60s...", flush=True)
    for i in range(120):
        time.sleep(0.5)
        if msg.state >= 3:
            break
    if "LK1" not in results:
        results["LK1"] = f"s{msg.state}"
    icon = "OK" if msg.state in (3, 4, 8, 255) else "??"
    print(f"  [{icon}] LK1: s{msg.state}", flush=True)

    # Send 3 more via link (should reuse established link)
    print("\n--- LK2: 3 messages over established link ---")
    time.sleep(5)
    for i in range(3):
        label = f"LK2-{i+1}"
        msg = LXMF.LXMessage(dest, source, f"LK2: Link msg {i+1}/3")
        msg.desired_method = LXMF.LXMessage.DIRECT
        def make_cb(l):
            def cb(m):
                results[l] = f"s{m.state}"
                print(f"  [{l}] -> s{m.state}", flush=True)
            return cb
        msg.register_delivery_callback(make_cb(label))
        router.handle_outbound(msg)
        time.sleep(10)
    print("  Waiting 30s for remaining deliveries...", flush=True)
    time.sleep(30)
    for i in range(3):
        label = f"LK2-{i+1}"
        s = results.get(label, "PENDING")
        icon = "OK" if any(x in s for x in ("s3", "s4", "s8", "s255")) else "??"
        print(f"  [{icon}] {label}: {s}", flush=True)


def run_lxmf_opportunistic_test(router, dest, source):
    """Test opportunistic delivery via LXMRouter."""
    print("\n" + "=" * 60)
    print("LXMF ROUTER: OPPORTUNISTIC TESTS")
    print("=" * 60)

    print("\n--- LO1: 5 messages, 5s apart ---")
    for i in range(5):
        label = f"LO1-{i+1}"
        msg = LXMF.LXMessage(dest, source, f"LO1 opp {i+1}/5")
        msg.desired_method = LXMF.LXMessage.OPPORTUNISTIC
        def make_cb(l):
            def cb(m):
                results[l] = f"s{m.state}"
            return cb
        msg.register_delivery_callback(make_cb(label))
        router.handle_outbound(msg)
        time.sleep(5)
    print("  Waiting 15s...")
    time.sleep(15)
    passed = 0
    for i in range(5):
        label = f"LO1-{i+1}"
        s = results.get(label, "PENDING")
        ok = any(x in s for x in ("s3", "s4", "s8", "s255"))
        if ok:
            passed += 1
        icon = "OK" if ok else "??"
        print(f"  [{icon}] {label}: {s}", flush=True)
    print(f"  LO1: {passed}/5 confirmed", flush=True)


def run_listen_mode(router, dest, source):
    """Listen for messages FROM the Ratdeck."""
    print("\n" + "=" * 60)
    print("LISTEN MODE — Waiting for messages from Ratdeck")
    print("=" * 60)
    print(f"\n  Our LXMF address (enter on Ratdeck):")
    print(f"  {source.hash.hex()}")
    print(f"\n  Send a message FROM the Ratdeck to this address.")
    print(f"  Listening for 120s...\n", flush=True)

    rx_before = len(messages_rx)
    for i in range(24):
        time.sleep(5)
        new = len(messages_rx) - rx_before
        if new > 0:
            print(f"  [{(i+1)*5}s] Received {new} message(s)!", flush=True)
            for m in messages_rx[rx_before:]:
                print(f"    \"{m['content'][:60]}\"", flush=True)
            results["LISTEN"] = "OK"
            break
    else:
        results["LISTEN"] = "NO_MSG"
        print("  No messages received from Ratdeck", flush=True)


# ============================================================
# SUMMARY
# ============================================================

def print_summary():
    print("\n" + "=" * 60)
    print("FINAL RESULTS")
    print("=" * 60)

    passed = failed = pending = 0
    for label in sorted(results.keys()):
        status = results[label]
        if status in ("DELIVERED", "OK") or any(x in str(status) for x in ("s3", "s4", "s8", "s255")):
            icon, passed = "OK", passed + 1
        elif status in ("FAILED", "TIMEOUT") or "s6" in str(status):
            icon, failed = "FAIL", failed + 1
        else:
            icon, pending = "??", pending + 1
        print(f"  [{icon}] {label:10s}: {status}")

    total = passed + failed + pending
    print(f"\n  PASSED:  {passed}/{total}")
    print(f"  FAILED:  {failed}/{total}")
    print(f"  PENDING: {pending}/{total}")
    print(f"\n  Messages received from Ratdeck: {len(messages_rx)}")
    for m in messages_rx:
        print(f"    \"{m['content'][:60]}\"")
    print()


# ============================================================
# MAIN
# ============================================================

def discover_mode(transport):
    print("=== DISCOVER MODE ===")
    print(f"Transport: {transport}\n")
    router, dd, identity = init_rns(transport)
    seen = set()
    for t in range(12):
        time.sleep(5)
        for dh in RNS.Identity.known_destinations:
            h = dh.hex()
            if h not in seen:
                seen.add(h)
                app_data = RNS.Identity.recall_app_data(dh)
                name = decode_name(app_data)
                marker = " <---" if name else ""
                print(f"  [{(t+1)*5:3d}s] {h}  \"{name}\"{marker}", flush=True)
    print(f"\n{len(seen)} destinations found.")
    print(f"Run: python3 {sys.argv[0]} <hash> [--tcp]")


def test_mode(dest_hex, transport, suite):
    dest_hash = bytes.fromhex(dest_hex.replace(":", ""))
    print(f"{'='*60}")
    print(f" LXMF COMPATIBILITY TEST SUITE")
    print(f"{'='*60}")
    print(f" Target:    {dest_hex}")
    print(f" Transport: {transport}")
    print(f" Suite:     {suite}")
    print(f"{'='*60}\n")

    router, dd, identity = init_rns(transport)

    if not wait_for_path(dest_hash):
        return False

    dest_id = RNS.Identity.recall(dest_hash)
    dest = RNS.Destination(
        dest_id, RNS.Destination.OUT, RNS.Destination.SINGLE, "lxmf", "delivery"
    )
    print(f"[OK] Destination ready (hops={RNS.Transport.hops_to(dest_hash)})\n")

    # --- RAW PACKET TESTS (firmware verification) ---
    if suite in ("raw", "all"):
        run_raw_size_tests(identity, dest, dest_hash)
        run_raw_unicode_tests(identity, dest, dest_hash)
        run_raw_timing_tests(identity, dest, dest_hash)

    # --- LXMF ROUTER TESTS (Python client compatibility) ---
    if suite in ("lxmf", "all"):
        run_lxmf_opportunistic_test(router, dest, dd)
        run_lxmf_link_test(router, dest, dd)

    # --- LISTEN MODE (Ratdeck → Python) ---
    if suite in ("listen", "all"):
        run_listen_mode(router, dest, dd)

    print_summary()
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="LXMF Compatibility Test Suite")
    parser.add_argument("dest_hash", nargs="?", help="Target destination hash (hex)")
    parser.add_argument("--tcp", action="store_true", help="Use TCP hub instead of LoRa")
    parser.add_argument("--suite", default="all",
                        choices=["raw", "lxmf", "listen", "all"],
                        help="Which test suite to run")
    args = parser.parse_args()

    transport = "tcp" if args.tcp else "lora"

    try:
        if args.dest_hash:
            test_mode(args.dest_hash, transport, args.suite)
        else:
            discover_mode(transport)
    except KeyboardInterrupt:
        print("\nInterrupted")
        if results:
            print_summary()
    except Exception as e:
        print(f"\n[ERROR] {e}")
        import traceback
        traceback.print_exc()
