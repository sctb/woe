: . (a -- ) p pop ;

: x ([a] -- [a]) dup e ;
: y ([a] -- [a]) [dup cons] swap cat dup cons e ;

: unit (a -- [a]) [] cons ;

: dip  (a [b] -- a) swap unit cat e ;
: dip2 (a b [c] -- a b) [dip] cons dip ;
: dip3 (a b c [d] -- a b c) [dip2] cons dip ;
: dip4 (a b c d [e] -- a b c d) [dip3] cons dip ;

: dig  (a b -- b a) [] cons dip ;
: dig2 (a b c -- b c a) [] cons cons dip ;
: dig3 (a b c d -- b c d a) [] cons cons cons dip ;

: bury  (a b -- b a) [[] cons] dip swap e ;
: bury2 (a b c -- c a b) [[] cons cons] dip swap e ;
: bury3 (a b c d -- d a b c) [[] cons cons cons] dip swap e ;

: dup2 (a b -- a b a b) [dup] dip dup bury2 ;
: dup3 (a b c -- a b c a b c) [dup2] dip dup bury3 ;

: keep (a [b] -- a) swap dup bury2 [e] dip ;
: keep2 (a b [c] -- a b) [dup2] dip dip2 ;
: keep3 (a b c [d] -- a b c) [dup3] dip dip3 ;

: when (? [t] -- ) swap [e] [pop] ? ;
: unless (? [f] -- ) swap [pop] [e] ? ;
: choice (? t f -- t/f) dig2 [pop] [swap pop] ? ;

: and (? ? -- ?) f choice ;
: or (? ? -- ?) t swap choice ;
: xor (? ? -- ?) unit dup [not] cat swap ? ;
: not (? -- ?) f t choice ;

: loop ([p] -- ) [e] keep [loop] cons when ;
: do ([p] [a] -- p [a]) dup dip2 ;
: while ([p] [a] -- ) swap do cat [loop] cons when ;
: until ([p] [a] -- ) [[not] cat] dip while ;
