: . p pop ;					\ a --

: x dup e ;					\ (a) -- (a)
: y (dup cons) swap cat dup cons e ;		\ (a) -- (a)

: unit () cons ;				\ a -- (a)

: dip  swap unit cat e ;			\ a (b) -- a
: dip2 (dip) cons dip ;				\ a b (c) -- a b
: dip3 (dip2) cons dip ;			\ a b c (d) -- a b c
: dip4 (dip3) cons dip ;			\ a b c d (e) -- a b c d

: dig  () cons dip ;				\ a b -- b a
: dig2 () cons cons dip ;			\ a b c -- b c a
: dig3 () cons cons cons dip ;			\ a b c d -- b c d a

: bury  (() cons) dip swap e ;			\ a b -- b a
: bury2 (() cons cons) dip swap e ;		\ a b c -- c a b
: bury3 (() cons cons cons) dip swap e ;	\ a b c d -- d a b c

: dup2 (dup) dip dup bury2 ;			\ a b -- a b a b
: dup3 (dup2) dip dup bury3 ;			\ a b c -- a b c a b c

: keep  swap dup bury2 (e) dip ;		\ a (b) -- a
: keep2 (dup2) dip dip2 ;			\ a b (c) -- a b
: keep3 (dup3) dip dip3 ;			\ a b c (d) -- a b c

: when swap (e) (pop) ? ;			\ ? (t) --
: unless swap (pop) (e) ? ;			\ ? (f) --
: choice dig2 (pop) (swap pop) ? ;		\ ? t f -- t/f

: and f choice ;				\ ? ? -- ?
: or t swap choice ;				\ ? ? -- ?
: xor unit dup (not) cat swap ? ;		\ ? ? -- ?
: not f t choice ;				\ ? -- ?

: loop (e) keep (loop) cons when ;		\ (p) --
: do dup dip2 ;					\ (p) (a) -- p (a)
: while swap do cat (loop) cons when ;		\ (p) (a) --
: until ((not) cat) dip while ;			\ (p) (a) --
