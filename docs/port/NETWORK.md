# Online play: the network library (`port/net`)

Written 2026-11-06 on branch `net-proto`. The plan is [NETPLAY.md](NETPLAY.md); this is its network part (NETPLAY.md
1.5-1.7, 3, phases 1, 2 and 5 of section 5, 8, and 11): sockets, the protocol, lockstep input exchange, the input
delay, time sync, the session's failures, the `RUSH_SIM_NET` network model, and their tests.

**Status.** Complete and tested on its own, against a toy deterministic simulation (section 9). **The game links it**
since branch `net-session` (`port/app/netplay.c`, [NETPLAY.md](NETPLAY.md) section 12: phase 1's wiring of section 10).
Two peers; the ports are a set per peer, ready for 3-4 players over 2-4 PCs later.

| File | What |
|---|---|
| `port/net/net.h`, `net.c` | the API and the session: handshake, lockstep, delay, time sync, hashes, ends, stats |
| `net_proto.h`, `net_proto.c` | the wire format: parsers that check every length, writers, the join code's tag |
| `net_sock.h`, `net_sock.c` | UDP sockets on POSIX and Winsock 2, addresses, `getaddrinfo` |
| `net_sim.h`, `net_sim.c` | `RUSH_SIM_NET` |
| `net_blake2s.h`, `net_blake2s.c` | BLAKE2s, written from RFC 7693 |
| `net_os.h`, `net_os.c` | the clock, sleeps, random bytes, a mutex, a condition variable, a thread (POSIX; Windows' own) |
| `port/net/tests/` | the toy simulation (`toy.c`), an in-memory network (`vnet.c`), a peer's loop (`driver.c`), `net_test`, `net_peer` |
| `tools/port/net_check.py` | two `net_peer` processes over loopback |

Everything here was written from the plan, RFC 7693, NTP's arithmetic and the POSIX and Windows Sockets
documentation. No code comes from emulator netplay, GGPO or other ports.

## 1. The API

`port/net/net.h` has the exact signatures; plain C types, no game headers.

```c
NetConfig c;  net_config_defaults(&c);          // role, ports, build, settings blob, files, delay, join code, ...
NetSession *s = net_create(&c, err, sizeof err);  // socket, peer resolved, handshake started
net_start_thread(s);                             // the service thread (or pump with net_poll / net_wait)
... NET_ACCEPT: net_peer_settings(), net_peer_file() checked and adopted, then net_accept(s)
net_start_time(s, &t0);                          // retrace 0's wall time on this side's clock

// each retrace n = 0, 1, 2, ...
net_submit(s, n, my_pads);                       // this side's pads (one NetPad per owned port), sampled now
while ((r = net_pads(s, n, pads, 10)) == 0)      // all four ports' pads of n, waiting up to 10 ms at a time
    pump_window_events(), show_waiting(net_waiting_ms(s) >= 250);
... run retrace n with pads ...
net_hash(s, n, state_hash);                      // (net_window_hash every 60 or so)
next_deadline += retrace_ns + net_pace_adjust_ns(s);   // see "The caller's clock" below

net_linger(s, 500); net_end(s, NET_END_LOCAL);   // BYE, once the peer has this side's last inputs
net_destroy(s);
```

`NetPad` is the N64 pad as `PcPad` has it: `uint16_t buttons; int8_t stick_x, stick_y;`, 4 bytes on the wire.
Ports nobody owns come back zero.

**Threads.** A session is one object behind one lock; every function may be called from any thread. Two ways to
drive it:

- **The service thread** (`net_start_thread`; the game's mode). It waits on the socket (`poll`/`ppoll`, `select` on
  Windows) up to the next timer, and does all the network's work: receiving, acks, resends, keepalives, pings,
  timeouts, `RUSH_SIM_NET`'s held packets. The main loop only submits, asks for pads and gives hashes;
  `net_pads` waits on a condition variable the thread signals when inputs arrive (POSIX: a `CLOCK_MONOTONIC`
  condition variable; Windows: `SleepConditionVariableCS`, since winpthreads' timed waits move in 15.6 ms steps).
  So the network is served while the main loop waits for the N64 threads or sleeps to a deadline.
- **Caller pumping**: `net_poll` (non-blocking) and `net_wait` (until a datagram or a timer); `net_pads` with a
  timeout pumps by itself. The deterministic tests drive two sessions this way, on a virtual clock (`now_ns` in the
  config) through an in-memory transport (`transport`).

The log callback runs with the session's lock held: it must not call back into the session.

**The caller's clock.** The library never sleeps for the game. The caller paces retraces:

- retrace n+1's deadline is deadline(n) + `retrace_ns` + `net_pace_adjust_ns()`;
- except after a stall: when `net_pads(n)` gave the pads more than half a retrace after deadline(n), the next deadline is the
  moment it gave them + `retrace_ns` + the adjustment (re-based: never racing to catch up);
- retrace 0's deadline is `net_start_time`, on the session's clock (`CLOCK_MONOTONIC`, QueryPerformanceCounter on
  Windows, or the `now_ns` given; the game should give its own, `pc_real_ns`, so the times are the same).

`net_pace_adjust_ns` is 0 to 1 ms (section 6); the paced clock's `pc_pace_adjust_ns` takes it as it is.

**Stats** (`net_stats`, `net_stats_line`): the state and end; RTT (average, min, max, jitter) and the clock offset;
this side's delay, its range and changes, the peer's; the slowest transit both ways; samples repeated or dropped by
delay changes; stalls, their total and longest time; time-sync corrections and their total; packets sent, received,
lost (inferred from sequence numbers), duplicated, reordered; resends, keepalives; packets rejected (and why: source,
tag, HELLO rate); hash and window-hash checks passed; the simulator's drops.

## 2. Sockets

- One UDP socket and port a session (`net_sock.c`). The host listens on `NetConfig.port` (default 24049); the guest
  takes any local port.
- **Dual-stack**: an IPv6 socket with `IPV6_V6ONLY` off, bound to `::`, so IPv4 and IPv6 peers both reach it; an IPv4
  peer seen through it (`::ffff:a.b.c.d`) is kept as IPv4. Where IPv6 is missing, or only IPv6-only sockets exist, an
  IPv4 socket instead. `bind` can name a local address (a Tailscale 100.x one).
- **Names**: `getaddrinfo`, so MagicDNS names (`rocky.tailfe03a6.ts.net`), 100.x addresses, IPv6 literals and
  `[v6]:port`. When a name has both, IPv4 is taken first (forwarded ports are usually IPv4).
- **Non-blocking**, 256 KiB buffers. Errors from ICMP (`ECONNREFUSED`, `WSAECONNRESET`) are skipped, not fatal.
  A datagram longer than the buffer is reported (`MSG_TRUNC` on Linux, `WSAEMSGSIZE`), never parsed truncated.
- **Windows**: Winsock 2 (`WSAStartup`, counted), `closesocket`, `ioctlsocket(FIONBIO)`, `WSAGetLastError`,
  `WSAIoctl(SIO_UDP_CONNRESET, FALSE)` so an ICMP port unreachable doesn't break the next `recvfrom`. `ws2_32` is
  linked; `pe_check.py` and [RELEASE.md](RELEASE.md) allow `ws2_32.dll`. The test programs import only KERNEL32,
  msvcrt, WINMM (`timeBeginPeriod`) and WS2_32.
- **Windows Defender Firewall** asks the first time the host listens: allow it on the network profile in use.
- **This PC's addresses** (the host's waiting screen, [NETPLAY.md](NETPLAY.md) 12.10): `net_local_addrs` lists the
  addresses a peer could reach, without loopback and link-local ones: `getifaddrs` with each interface's name on
  Linux, `getaddrinfo` on the computer's own name on Windows (its permanent addresses; nothing beyond `ws2_32`).
  `net_addr_kind` names an address by its range (Tailscale's 100.64.0.0/10 and fd7a:115c:a1e0::/48, a private
  network's, public, loopback, link-local). The MagicDNS name: `net_dns_ptr` asks one DNS server for an IPv4
  address's PTR over UDP (the question and the answer, with compression pointers, built and read here:
  `net_dns_ptr_query`, `net_dns_ptr_answer`); the screen asks Tailscale's resolver, 100.100.100.100, since the
  system's resolver may answer for its own addresses with the bare computer name. `net_addr_name` (getnameinfo) and
  `net_host_names` (the canonical name) are its fallbacks. `net_peer_addr` gives the peer's address once known.

## 3. The wire format

Little-endian, fixed layouts. A packet is a 40-byte header, a payload, and with a join code a 16-byte tag; at most
**1200 bytes** (Tailscale's MTU is 1280). The parser checks the length against every count, rejects a packet with
bytes left over, and text fields must be NUL-terminated printable ASCII.

**Header** (every packet):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `R2NP` |
| 4 | 1 | protocol version (1) |
| 5 | 1 | type (below) |
| 6 | 1 | flags: bit 0 = a tag follows the payload; the rest 0 |
| 7 | 1 | sender: 0 host, 1 guest |
| 8 | 8 | session ID (0 in HELLO; in HELLO_ACK, the session offered) |
| 16 | 4 | sequence number (per sender) |
| 20 | 8 | the sender's clock at sending, ns |
| 28 | 8 | echo: the send time of the last packet the sender received from the receiver (0: none) |
| 36 | 4 | hold: µs from that packet's arrival to this send |

**Messages:**

| Type | Name | Payload |
|---|---|---|
| 1 | HELLO | guest to host, every 100 ms until answered |
| 2 | HELLO_ACK | host to guest |
| 3 | SETTINGS | a chunk of the host's settings blob |
| 4 | FILE | a chunk of a file |
| 5 | FILE_ACK | which chunks of a blob have arrived |
| 6 | READY | the agreement hash (32 bytes) |
| 7 | START | host to guest |
| 8 | INPUT | inputs, acks, schedule, hashes (every retrace, and as keepalive) |
| 9 | HASH | INPUT's layout with no inputs (accepted; v1 sends its hashes inside INPUT) |
| 10, 11 | PING, PONG | `u32 id` (the header's times are the measurement) |
| 12 | BYE | `u8 reason`, 3 reserved, `u32 detail` (the desync's retrace), `char text[64]` |

HELLO and HELLO_ACK (104 + 36 × files bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | nonce (random) |
| 8 | 8 | HELLO_ACK: the guest's nonce; HELLO: 0 |
| 16 | 1 | the ports the sender owns (bit i = port i + 1; nonzero, low 4 bits) |
| 17 | 1 | its file count, 0-8 |
| 18 | 2 | 0 |
| 20 | 4 + 32 | its settings blob: size (at most 64 KiB), BLAKE2s-256 hash |
| 56 | 48 | build (the commit), NUL-terminated |
| 104 | 36 each | each file: size (at most 1 MiB), BLAKE2s-256 hash |

SETTINGS and FILE (12 + length): `u8 index`, `u8 0`, `u16 chunk`, `u32 total size`, `u16 length` (1024, the last
chunk the rest), `u16 0`, the bytes. FILE_ACK: `u8 kind` (0 settings, 1 file), `u8 index`, `u16 chunks`, then a bitmap
of the chunks received (bit i of byte i/8).

START (52): the agreement hash (32), retrace 0's time on the host's clock (`u64` ns), the host's clock offset
(`i64` ns, guest minus host), `u8` offset valid, 3 reserved.

INPUT and HASH (52 + count × (4 × ports + 2) + 4 × hashes, + 12 with a window hash):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | cur: the last retrace the sender submitted (0xFFFFFFFF before retrace 0) |
| 4 | 8 | cur_start: when it submitted it, its clock |
| 12 | 4 | ack: the receiver's inputs the sender has, contiguous from 0 (the next it needs) |
| 16 | 4 | hash_ack: the same for hashes |
| 20 | 4 | whash_ack: the last window hash received, its retrace + 1 |
| 24 | 4 | first: the retrace of the first input here (= the receiver's last ack) |
| 28 | 1 | count, at most 120 |
| 29 | 1 | ports (the sender's) |
| 30 | 1 | the sender's input delay now (for the stats) |
| 31 | 1 | flags: 1 a window hash follows, 2 feedback valid, 4 clock valid |
| 32 | 4 | feedback: the slowest transit of the receiver's inputs over the last 3 s, as the sender measured it, µs |
| 36 | 8 | clock: the sender's offset (receiver's clock minus its own), ns; the host's is the one both use |
| 44 | 4 | hash_first |
| 48 | 2 | hash count, at most 256 |
| 50 | 2 | 0 |
| 52 | count × 4 × ports | each input: per port `u16 buttons`, `s8 x`, `s8 y` |
| | count × 2 | each input's age at sending, 100 µs units (from its assignment) |
| | hashes × 4 | the state hash of retraces hash_first, hash_first + 1, ... |
| | 12 | with flag 1: `u32 retrace`, `u64` window hash |

An INPUT is built to fit 1200 bytes: inputs first (oldest first), then hashes. At its largest, 120 inputs of one port,
94 hashes and a window hash, it is 1200 bytes; a tagged FILE chunk is 1092.

**The join code's tag** (phase 5, implemented): with `NetConfig.join_code` set, every packet carries a 16-byte tag,
BLAKE2s-128 keyed with K over the packet before it, where K = BLAKE2s-256(`"rush2049pc join code v1\0"` + the code).
A side with a code drops every packet whose tag is missing or wrong; a side without one drops tagged packets. With
different codes the guest gets no answer and ends with `no-answer` after its timeout. The tag authenticates; it
doesn't encrypt. A short code can be guessed offline from one captured packet: use a long one (or Tailscale, whose
WireGuard already authenticates and encrypts).

**The agreement hash** (READY, START): BLAKE2s-256 over a label, the protocol version, the build, both port sets, the
host's settings blob's size and hash, and every file's size and hash, the host's then the guest's. Both sides
compute it from the HELLOs; the blobs themselves are checked against their hashes as they arrive.

## 4. The session

`connecting` → `handshake` → `accept` → `ready` → `starting` → `running` → `ended`.

1. **HELLO.** The guest sends HELLO every 100 ms (build, ports, its files' sizes and hashes, its own settings hash).
   The host checks the build (**exactly** the same string, else BYE `version` with its own build, and the guest ends
   with "the host runs build X; this is Y: both players need the same build"), that the ports don't overlap, and the
   sizes. It doesn't take the guest yet: a HELLO can be replayed from any address. Each sender gets a HELLO_ACK
   with a session of its own (up to 4 pending), and the first address to send a packet in its session (tagged, with
   a code) becomes the peer. From then on the host ignores every other address.
2. **Handshake.** The host sends its settings blob, each side its files, in 1 KiB chunks, 32 chunks a pump at most,
   each resent every 100 ms until a FILE_ACK has it. A blob whose hash doesn't match ends the session (`protocol`).
   Pings every 50 ms meanwhile. The guest compares the host's settings with its own (`net_settings_differ`); the host's
   are the session's.
3. **Accept.** With everything received, the session waits for the caller (`net_accept`), which checks the files
   (`pc_pak_image_check` in the game) and adopts the settings; `auto_accept` skips this.
4. **READY** (every 100 ms) carries the agreement hash. A different one ends both (`mismatch`).
5. **START.** With the guest's READY, its own acceptance and 4 round trips measured, the host picks retrace 0's time:
   now + 2 × the handshake's slowest round trip + 100 ms (250 ms to 3 s). START carries it and the host's clock
   offset; the guest moves it to its own clock. The host repeats START until an INPUT comes back.
6. **Running**: section 5.
7. **Ending.** `net_end` sends BYE three times; a peer that keeps sending after the end gets BYE again (5 a second at
   most). Reasons: `local`, `peer-left` (BYE), `timeout` (nothing from the peer for `timeout_ms`, 15 s), `no-answer`
   (the guest, after `timeout_ms`), `protocol` (two values for one input or hash, a bad file, no acks), `desync`
   (the detail is the first retrace whose hashes differ), `version`, `mismatch`, `rejected`, `busy`, `socket`.
   `net_end_reason` gives a sentence for the log; `net_end_text` ends with a sentence of the caller's, which the BYE
   carries to the peer (its first 63 characters: "the guest's ROM isn't the host's").
8. **Leaving at a retrace of its own** (a player quits, a test's run reaches its limit): `net_linger` first waits, up
   to a timeout, until the peer has acknowledged every input this side assigned; after the BYE, `net_pads` still gives
   the peer the retraces the side that left ran itself (up to the last it reported submitting) when their pads arrived
   (`drained`), so the peer reaches the retrace the other left at; before the BYE came it may have run up to the
   other's delay past it, on the inputs the other had assigned that far ahead (their hashes are compared by nobody).
   The first retrace past them ends the peer with `peer-left`.

**Waiting for the peer.** A `net_pads` that has waited `wait_report_ms` (250) is logged ("waiting for the peer's input
for retrace N") and `net_waiting_ms` says how long, for the "Waiting for <name>..." overlay; when the input comes, the
log says after how long.

## 5. Lockstep inputs and the input delay

**Assignment.** At retrace n the caller submits this side's sample; the library assigns it to retrace n + D, D being
this side's delay now:

- D grew since the last retrace: the retraces skipped get the last input assigned again (a sample repeated);
- D shrank: n + D already has an input, and this sample is dropped;
- before retrace 0, retraces 0 .. D-1 get idle input (zeros), assigned and sent at START.

So the delay is each side's own business and needs no agreement: the other side only needs retrace N's input before
N. `net_test unit` checks it with inputs that carry their retrace: each side's port shows its samples in order, one
repeated where its delay grew and one skipped where it shrank, and the other side sees exactly the same.

**Exchange.** Each submit sends an INPUT at once, and a retrace with no INPUT sent gets one anyway (keepalive). An
INPUT carries every input the peer hasn't acknowledged (at most 120), so one lost packet costs nothing; a burst of
losses costs a retrace each until one gets through. Inputs are stored by retrace in rings of 1024; the first copy
wins; a duplicate must match it, and a different value is a protocol error that ends the session. Duplicates and
reordering are harmless. Every INPUT also carries the sender's hashes the peer hasn't acknowledged; each side compares
as both are known and ends both on the first mismatch, naming its retrace; window hashes (`net_window_hash`) likewise.

**Transit.** For each input arriving here for the first time, `transit = arrival − (its assignment on the peer's clock
− the clock offset)`: the network's delay, loss recovery included, but neither side's schedule. (The age field gives
the assignment time exactly, delay changes and repeats included.) Not measured: inputs assigned before the start (they
waited for START, not on the network), and anything read after a gap of 50 ms in pumping (a stopped process, a
hitch: their arrival time is unknown; the flag goes through `RUSH_SIM_NET`'s queue too).

**The delay controller** (each side for itself, every submit):

- *need* = the larger of the slowest transit of the peer's inputs here over the last 3 s, and the peer's feedback:
  the slowest transit of this side's inputs there over the last 3 s;
- *target* = ceil((need + margin) / retrace), margin 3 ms, clamped to [`delay_min`, `delay_cap`] = [1, 30];
- target above D: D = target at once (grows fast; the inputs skipped repeat);
- target below D: D − 1, only once the target has stayed below for 2 s and 2 s after the last change (shrinks one
  retrace every 2 s at most);
- nothing measured lately (a long stall): no change;
- `delay_fixed` (`--net-delay N`) fixes it;
- the first D, at START: the handshake's slowest round trip / 2 + margin.

Past the cap the game stalls instead. In the plan's terms (NETPLAY.md 11.2): RTT/2 is the first delay, and with
the shared clock offset each direction's transit is what RTT/2 estimates, measured per input; the jitter is the 3 s
maximum of those transits, loss recovery included (a decaying maximum by window rather than by factor); and a stall
grows the delay when it is the network's, since an input that arrives late arrives with a long transit. A stall that
isn't the network's is not a reason to grow: if transit is normal, it came from one side's schedule (a hitch, a
stopped process), and time sync re-aligns; growing would leave a long delay behind for nothing (the `pause` and
`sigstop` tests check the delay stays put). Lateness measured against need instead mixes the two, which is why this
measures transit.

## 6. Time sync and the clock offset

**The offset.** Every packet's header is an NTP exchange: the echo is t1, its arrival there t2 = (this packet's send
time − hold), this packet's send time t3, its arrival here t4; delay = (t4 − t1) − hold, offset = ((t2 − t1) +
(t3 − t4)) / 2. Per second the least-delayed sample is kept (32 seconds); the offset is that of the best whole second,
an older second counting as 50 ppm of its age worse (clock drift). Whole seconds only: when the latency drops, one
lopsided sample (one direction still slow) is beaten within its second by the ones after it.

**One offset for both.** The host's offset is the one both sides use: START and every host INPUT carry it, and the
guest takes its negation. Two independent estimates disagree by their noise, and then each side can believe it is
ahead and both slow down; measured before the change: 59.82 ticks a second at 80 ms ± 20 instead of 60.000. With one
offset, both sides' views are each other's mirror: only one can be ahead. And a bias in the offset (an asymmetric
path) moves the alignment and the transits by the same amount, so the two delays together still cover the round
trip.

**Time sync.** `net_pace_adjust_ns`, called once a retrace after `net_pads(n)`: this side's next retrace begins at
next = start(n) + retrace (or the pads' arrival + retrace after a stall); the peer's projected next = the start of its
last reported retrace on this side's clock + whole retraces since. When this side's next would come more than 1 ms
before the peer's, it is ahead: it adds half the difference to the interval, at most 1 ms. It never subtracts: the side
behind doesn't race, the side ahead waits. Between PCs whose crystals differ by tens of ppm the faster stretches now
and then, and the game runs at the slower one's 60 Hz.

## 7. Hostile packets

- Every packet: length, magic, version, type, flags, sender; text NUL-terminated and printable; counts within the
  bytes there are; no bytes left over; at most 1200 bytes; datagrams longer than the buffer dropped.
- The tag first, when there is a join code; then the source: once the peer is known, any other address is dropped
  (`rejected_source`); then the session ID.
- Values against the session: the peer's ports; inputs within 1024 retraces of what is being consumed; the current
  retrace consistent with them; hashes only for retraces run; acks no further than what was sent. Anything else is
  dropped, not fatal: only a conflicting input or hash, from the authenticated peer, ends the session.
- HELLOs: at most 50 a second from everyone (each answer no bigger than the HELLO), 4 pending; a replayed HELLO
  gets its sender a session it can't use without the code.
- Without a join code, anyone who can reach the port and speaks first can join, and an attacker who can send from the
  peer's address with its session ID can inject inputs. That is what Tailscale (WireGuard) or the join code is for.
- Nothing from the network is a path; the files are opaque bytes for the caller to check.
- Not handled: a peer whose address changes mid-session (a NAT rebinding) is a timeout.

## 8. `RUSH_SIM_NET`

A bad network in a session's receive path, as the project's other `RUSH_SIM_*` models: every datagram read goes
into a queue with a release time, seeded and reproducible (`seed=`, or from the session's role). Comma-separated:

| Key | Meaning |
|---|---|
| `delay=MS` | one-way delay added to every packet received |
| `jitter=MS` | plus a uniform draw in [−MS, +MS] (packets overtake each other) |
| `loss=PCT`, `dup=PCT` | dropped; delivered twice (the copy with its own delay) |
| `reorder=PCT` | held back 5-40 ms more |
| `at=S` | the keys after it apply from S seconds after the session was made |
| `burst=E:L:PCT` | every E s, for L s, PCT loss |
| `spike=E:L:MS` | every E s, for L s, MS more delay |
| `pause=AT:LEN` | this peer stops at AT s for LEN s: sends nothing; what arrives waits in the socket; `net_frozen` tells the caller to stop too |
| `seed=N` | the draws' seed |

`NetConfig.sim` sets it per session (the in-process tests); otherwise the environment's. With both processes given
the same model, both directions have it.

## 9. Tests

`ctest -R ^net_` (port/net's CMakeLists.txt; also standalone: `cmake -S port/net -B build/net`):

| Test | Kind | What |
|---|---|---|
| `net_unit` | 0.3 s | BLAKE2s (RFC 7693's vector and the reference KATs, the same as Python's `hashlib.blake2s`); every message round trip and every truncation; sizes; addresses, their kinds, this PC's own (none loopback or link-local, each once), the DNS PTR question and answers (a compression pointer, a loop, NXDOMAIN, a cut one); the simulator's parsing, seeding and pause; sockets on 127.0.0.1 and ::1 (dual-stack, an IPv4 sender kept as IPv4, oversized datagrams, ICMP unreachable); on a virtual clock: the handshake with settings and 5 files at 20% loss, BYE, the timeout, a desync, builds that differ, join codes (same, different, none), a replayed HELLO, a fixed delay, spoofed and hostile packets, the assignment rules across delay changes |
| `net_sim_<model>` | virtual clock, 10,000 ticks each | the same hash at every tick on both; stalls bounded; 60.000 a second where the delay covers the latency |
| `net_fuzz` | 2 s | 300,000 random and mutated packets through the parser; six sessions fed 33,000 random and mutated packets each, with and without a join code |
| `net_threads` | real time, 9 s | two threads over UDP on 127.0.0.1, 80 ms ± 20, the service threads |
| `net_loopback_<model>` | real time, 8-15 s | two `net_peer` processes (`tools/port/net_check.py`) |
| `net_desync`, `net_disconnect`, `net_bye`, `net_sigstop` | real time, 4-17 s | two processes: a flipped bit at tick 300; the guest killed; the guest quitting; the guest stopped with SIGSTOP for 3 s (Linux) |

The models (each direction; `tools/port/net_check.py` has the real-time ones, with the changes sooner):

| Model | `RUSH_SIM_NET` |
|---|---|
| clean | none |
| lan20 | `delay=20` |
| wan80 | `delay=80,jitter=20` |
| bad150 | `delay=150,jitter=40,loss=10,dup=2,reorder=2` |
| shift | `delay=10,at=60,delay=200,at=110,delay=30` (real time: at 3 and 7 s) |
| pause | the guest `delay=20,pause=60:3` (real time: at 4 s), the host `delay=20` |
| burst | `delay=30,jitter=5,burst=5:1:30` (real time: 0.5 s every 3 s) |

**Results** (rocky, x86-64, Debug and RelWithDebInfo, 2026-11-06: 21 of 21 in each; the virtual runs are the same
on the Pi 5). Virtual clock, 10,000 ticks:

| Model | Same hash every tick | Delay (retraces) over time | Stalls, host + guest | Ticks/s, last 3/4 |
|---|---|---|---|---|
| clean | yes, 10,000 + 10,000 checks | 1 | 0 | 60.000 |
| lan20 | yes | 2 | 0 | 60.000 |
| wan80 | yes | 6, then 7 | 0 | 60.000 |
| bad150 | yes | 10-13, mostly 12-13 | 1 + 2, 19 ms in all | 59.995 |
| shift | yes | 1; 13 at 200 ms; down to 2 within 25 s of the drop to 30 ms | 2 + 2: the jump to 200 ms, 184 ms each | 59.824 (the jump's stalls) |
| pause | yes | 2 throughout | 1 (the host, 2.98 s) | 58.6 (the pause) |
| burst | yes | 3-6 | 3 + 2, 70 ms in all | 59.985 |

Real time over UDP on rocky (two processes, RelWithDebInfo; ticks a second from the first third on, host / guest):

| Model | Ticks | Same hash every tick | Delay | Stalls (ms in all) | Ticks/s |
|---|---|---|---|---|---|
| clean | 420 | yes | 1 | 0 / 0 | 60.000 / 60.000 |
| lan20 | 420 | yes | 2 | 0 / 0 | 60.000 / 60.000 |
| wan80 | 480 | yes | 5-7, then 7 | 2 (15) / 1 (4) | 59.999 / 60.000 |
| bad150 | 600 | yes | 11-13 | 1 (22) / 0 | 60.000 / 59.990 |
| shift | 720 | yes | 1; 13 from the jump; 12 | 2 (367) / 2 (367), at the jump | 60.000 / 60.000 |
| pause | 600 | yes | 2 | 1 (2,976: the pause) / 0 | 41.4 (the pause) |
| burst | 480 | yes | 2-5 | 2 (20) / 3 (28) | 59.737 / 59.996 |

`net_threads` (80 ms ± 20, 480 ticks): the same hashes, 60.000 a second, delay 7, no stall, BYE across in 70 ms.
Desync: both end `desync` at retrace 300, each naming both hashes. Disconnect: the host ends `timeout` 3.02 s after
the kill (a 3 s timeout in the test; 15 s by default). BYE: the host ends `peer-left` 0.04 s after the guest exits.
SIGSTOP: the host stalls 2.98 s, the run completes with the same hashes at all 720 ticks, and the delay stays 2.

**A soak** (`net_check.py soak --model bad150 --ticks 36000`, the Pi, 10 minutes): the same hash at all 36,000 ticks;
stalls 14 + 10 (177 ms in all, the longest 21 ms); the delay 11-15; 59.983 ticks a second.

**Sanitizers** (the Pi, ASan and UBSan): `net_test unit`, `fuzz`, the bad150, shift, pause and burst models, `threads`, and the two-process
bad150, clean, desync and SIGSTOP cases: no report, and 60.000 a second under ASan.

**The whole suite** (rocky, Debug, `ctest -j3`): 393 of 393 (`assets_rom` skipped: it needs the ROM), on the branch
before the last port/net-only changes, whose net tests were run again after them.

**Windows.** The library and both test programs cross-build with mingw-w64 (the Pi, Debian 12's GCC 12.2), no
warnings, importing only Windows' own DLLs; the tests are in ctest, so CI's `windows` job (MSYS2) runs them. They
haven't been run on Windows here.

**What the tests found** (each fixed, with its test):

- Each side estimating the clock offset on its own: both believed they were ahead and slowed (59.82 a second at
  80 ms ± 20). Fixed by the host's offset for both (section 6).
- When the latency dropped from 200 to 30 ms, a lopsided timestamp sample moved the offset by 85 ms and the delay
  overshot to 18 before coming down. Fixed by whole-second buckets.
- The idle inputs of the start, assigned before START reached the guest, counted as 150 ms of transit: a first delay
  of 10 at 30 ms. They aren't measured now.
- A real SIGSTOP (not the simulated pause) drove the delay to its cap: the packets read late were held again by the
  simulator's queue and measured as 3 s of transit. The "read late" flag now goes through the queue.
- A stranger replaying the guest's HELLO could become the host's peer (the fuzzer found it on x86-64, where its
  draws came in another order). Fixed by the pending sessions of section 4.
- Hashes were sent from the wrong starting point and a side's checks could stop; the inferred loss could underflow.

## 10. Attaching the game (phase 1)

Done on branch `net-session`: [NETPLAY.md](NETPLAY.md) section 12 has what was built and run. The plan was, wiring
only:

- `port/ultra/loop.c`, at the retrace (NETPLAY.md 1.3): after `pc_input_poll`, `net_submit(s, vi, &pads[local])`;
  then `net_pads(s, vi, merged, 10)` in a loop that pumps window events and shows the waiting overlay from
  `net_waiting_ms`; `pc_si_set_pads(merged)`; after the retrace's `snapshot()`, `net_hash(s, vi, curated_hash)` and
  every 60 `net_window_hash`.
- The paced clock (`net-clock`): its epoch from `net_start_time` (the session's `now_ns` set to `pc_real_ns`), each
  interval plus `net_pace_adjust_ns()`, the re-base rule of section 1.
- `port/main.c`: `--net-host [PORT]`, `--net-join HOST[:PORT]`, `--net-delay auto|N`, `--net-code CODE`; the
  settings blob (the must-match list, NETPLAY.md 1.5) and the paks as files; `net_accept` after
  `pc_pak_image_check`.
- CMake: `pcnet` into `PC_PORT_OBJECT_LIBS` (or linked whole); `rush2049pc.exe` then imports ws2_32.dll, which
  `pe_check.py` already allows.

## 11. Not done, and open

- Attached to the game (section 10, NETPLAY.md 12), on the paced clock.
- Two peers. The ports are a set per peer and the wire carries the sender's ports, but sessions of three or four
  PCs need the host to relay or a mesh.
- No address migration: a NAT rebinding mid-session ends it by timeout.
- No rendezvous server or relay (phase 5's later steps).
- The margin (3 ms), the windows (3 s, 2 s) and the time-sync deadband (1 ms) are set from the simulator; phase 3's
  sessions between real PCs should check them, with the log's delay and stall lines.
- Real-time runs on Windows: compiled and linked here, run only by CI.
