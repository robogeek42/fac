 10 MODE 3
 60 DIM bigmap%(15*3,15*3)
 70 DIM smallmap%(15,15)

 100 PROCloadmap("fmap_tl.data")
 110 PROCcopymap(0, 0)
 120 PROCloadmap("fmap_t.data")
 130 PROCcopymap(15, 0)
 140 PROCloadmap("fmap_tr.data")
 150 PROCcopymap(30, 0)

 160 PROCloadmap("fmap_l.data")
 170 PROCcopymap(0, 15)
 180 PROCloadmap("fmap_m.data")
 190 PROCcopymap(15, 15)
 200 PROCloadmap("fmap_r.data")
 210 PROCcopymap(30, 15)

 220 PROCloadmap("fmap_bl.data")
 230 PROCcopymap(0, 30)
 240 PROCloadmap("fmap_b.data")
 250 PROCcopymap(15, 30)
 260 PROCloadmap("fmap_br.data")
 270 PROCcopymap(30, 30)

 290 PRINT "save map"
 300 A = OPENOUT "fmap.data"
 320 FOR Y%=0 TO 15*3-1
 330 FOR X%=0 TO 15*3-1
 340 BPUT#A,bigmap%(Y%,X%)-10
 350 NEXT
 360 NEXT
 390 CLOSE#A

1000 END
 
 2290 DEF PROCloadmap(FILENAME$)
 2300 LOCAL XIMP%,YIMP%
 2330 fnum=OPENIN FILENAME$
 2340 IF fnum=0 THEN PRINTTAB(7,14);"Filename NOT loaded":GOTO2480
 2350 INPUT#fnum,XIMP% :REM Read X map size
 2360 INPUT#fnum,YIMP% :REM Read Y map size
 2370 INPUT#fnum,decks% :REM load the number of decks in use
 2380 INPUT#fnum,custom% :REM read number of custom tile slots
 2390 FOR i=0TOXIMP% :REM read map based on defined size
 2400   FOR j=0TOYIMP%
 2410     INPUT#fnum,smallmap%(i,j) :REM save to map
 2420   NEXT
 2430 NEXT
 2480 CLOSE#fnum
 2590 ENDPROC

 2700 DEF PROCcopymap(A%,B%)
 2710 FOR Y%=0 TO 14 
 2720 FOR X%=0 TO 14
 2730 bigmap%(B%+Y%,A%+X%) = smallmap%(Y%,X%)
 2740 NEXT
 2750 NEXT
 2760 ENDPROC
