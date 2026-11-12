# Security

WAL++ parses trace files and evaluates WAL scripts. Treat a WAL script like
any other program and only run scripts you trust. A trace file, however,
should never be able to crash WAL++ or make it run code.

If you find a problem with security impact, for example a crafted VCD/FST/CSV
file that causes memory corruption, please do not open a public issue. Report it
privately through GitHub's "Report a vulnerability" button on the Security tab
of <https://github.com/iti-luebeck/WALpp>, or by e-mail to
r.meyer@uni-luebeck.de. We aim to reply within two weeks.

Only the latest release receives fixes.
