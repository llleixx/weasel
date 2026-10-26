Server candidate UI regression tests

Run test\ServerUI\run-tests.cmd from a Visual Studio x64 developer command
prompt, and again from an x86 prompt. Set BOOST_ROOT to the same Boost tree
used by weasel.props. The normal repository build prerequisites apply.
No user-specific path, elevated privilege, installed Rime engine, named-pipe
service, or production input-method installation is required.

The runner builds the real IPC/UI libraries, compiles the tests, and executes
each child process with a 15-second timeout. Results are written under
msbuild\ServerUITests\<platform>. NotificationTest and NativePresentationTest
require a Windows desktop session with DirectComposition support. They may
briefly show a synthetic candidate panel and never type into another app.

Coverage:
- CommandDispatchTest invokes the actual native and IPC command dispatchers.
  The IPC path enters with the production mutex already held. This catches
  recursive acquisition (which can throw or deadlock depending on the CRT).
- NotificationTest checks timeout state before the window thread processes
  a snapshot, and immediate cancellation.
- StateTest checks TSF host policy, composition and focus independently,
  plus stale candidate revisions and actions after EndUI.
- ResponseSnapshotTest overwrites the source buffer before using an earlier
  response, as happens when TSF delays granting an edit lock. It also checks
  revision parsing and rejection of an older acknowledgement after a newer
  response has been produced.
- PositionTest checks full signed screen coordinates and rejects malformed
  or out-of-range wire values.
- NativePresentationTest checks asynchronous publication, window-thread
  ownership, coalesced visibility, atomic batches, lifetime and timeout.

These are focused regression tests, not a substitute for integration tests
in real TSF hosts. Before merging a rendering migration, test candidate
clicks, rapid typing and document switching in a desktop editor, a browser
and a game overlay, as well as backend restart and mixed-version fallback.
Immersive hosts retain their existing view-owned candidate window path.
