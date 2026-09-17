# Threats

**Not written yet, because there is nothing listening on a network yet.** It is
written in Phase 6, alongside the server.

When it exists, this file says what is defended, from whom and how; what is
deliberately not defended, and why; and the order the work was done in. Every
message the server accepts is named in it.

One property is fixed already by the design rather than by any code: clients
send inputs, never state, so a client cannot claim to be somewhere it is not.
That does not make inputs trustworthy, and their validation belongs here when
it exists.
