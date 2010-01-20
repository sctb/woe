: . (a -- ) print pop ;

: dup2 dup dup ;
: dup3 dup dup dup ;

: dig  (a b -- b a) [] cons dip ;
: dig2 (a b c -- b c a) [] cons cons dip ;
: dig3 (a b c d -- b c d a) [] cons cons cons dip ;

: bury  (a b -- b a) [[] cons] dip swap i ;
: bury2 (a b c -- c a b) [[] cons cons] dip swap i ;
: bury3 (a b c d -- d a b c) [[] cons cons cons] dip swap i ;

: dip2 (a b [c] -- a b) [unit cons] dip dip i ;
: dip3 (a b c [d] -- a b c) [unit cons cons] dip dip i ;
: dip4 (a b c d [e] -- a b c d) [unit cons cons cons] dip dip i ;

: keep (a [b] -- a) swap dup bury2 [i] dip ;
: keep2 (a b [c] -- a b) [dup2] dip dip2 ;
: keep3 (a b c [d] -- a b c) [dup3] dip dip3 ;

: when (? [t] -- ) swap [i] [pop] branch ;
: unless (? [f] -- ) swap [pop] [i] branch ;
: choice (? t f -- t/f) dig2 [pop] [swap pop] branch ;

: and (? ? -- ?) false choice ;
: or (? ? -- ?) true swap choice ;
: xor (? ? -- ?) unit dup [not] cat swap branch ;
: not (? -- ?) false true choice ;

: loop ([p] -- ) [i] keep [loop] cons when ;
: do ([p] [a] -- p [a]) dup dip2 ;
: while ([p] [a] -- ) swap do cat [loop] cons when ;
: until ([p] [a] -- ) [[not] cat] dip while ;
