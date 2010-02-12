#!/usr/bin/awk -f 

# Small script to parse the UsbSnoop.log file and split
# fingerprint dump according to Damir findings
# Original author: Francesco Zuliani (aka zullinux)


BEGIN {
    currrec=0;
    RECDEFLEN=292;
    SEPLEN=2;
    DOUBLERECSEP=49;
    onlyfirsttime=1;
}

/TransferBufferLength/ {
    hexvalstr="0x" $3;
    tlen=strtonum(hexvalstr);
}

tlen > 0x8000 {
    printf "/* TransferBufferLength > 0x8000 */ \n";
    getline;
    while ($0 !~ /UrbLink/) {
	if ($1 ~ /[:xdigit:]/ ) {
	    check($0, onlyfirsttime);
	}
	getline;
    }
    if (started) {
	printf "\n/* LAST    RECORD rec= %d len is %d partial= %d LINE: %d */\n", currrec, reclen, partialdoublerec, NR ;
	checkconsistency(currrec, reclen);
	started=0;
    }
    tlen=0;
}

END {
    printf "\n} ;\n";
}

function check(currline, firsttime ) {
    if (onlyfirsttime==1) {
	printf "/* THIS IS ONE: %d (0x %x) %d */ \n", tlen , tlen, NR ;
	printf "unsigned char finger[] = {\n";
	onlyfirsttime=0;
    }
    for (i=2; i<=NF; i++) {
	if (missingcomma) {
	    printf ", " ;
	    missingcomma=0;
	}
	reclen++;
	if ($i == "01" )  {
	    half_rs=1;
	}
        else if (half_rs==1) {
	    if ($i == "fe") {
		if (started) {
		    printf "\n/* END OF   RECORD rec= %d len is %d partial= %d LINE: %d */\n", currrec, reclen, partialdoublerec, NR ;
		    checkconsistency(currrec, reclen);
		}
		currrec++;
		partialdoublerec++;
		printf "\n/* STARTING RECORD rec= %d LINE: %d */\n", currrec, NR ;
		printf "0x01, 0xfe" ;
		if (i != NF) printf ", " ;
		else missingcomma=1
		reclen=2;
		half_rs=0;
		started=1;
	    }
	    else {
		printf "0x01, 0x%s", $i ;
		if (i != NF) printf ", " ;
		else missingcomma=1
		half_rs=0;
	    }
	}
	else {
	    printf "0x%s", $i ;
	    if (i != NF) printf ", " ;
	    else missingcomma=1
	    half_rs=0;
	}
    }
}

function checkconsistency(mycurrrec, myreclen) {
    if (myreclen == RECDEFLEN+SEPLEN) {
	printf "/* NORMAL  RECORD reclen= %d rec= %4d */\n", myreclen, mycurrrec;
    } else if (myreclen == 2*RECDEFLEN+SEPLEN) {
	if (mycurrrec-prevdouble == DOUBLERECSEP) {
	    printf "/* DOUBLE  RECORD reclen= %d rec= %4d diff= %d partial= %d */\n", myreclen, mycurrrec, mycurrrec-prevdouble, partialdoublerec;
 ;
	} else {
	    printf "/* WRONG PLACE FOR DOUBLE  RECORD reclen= %d rec= %4d diff= %d != %d partial= %d */\n", myreclen, mycurrrec, mycurrrec-prevdouble, DOUBLERECSEP , partialdoublerec;
	}
	partialdoublerec=0;
	prevdouble=mycurrrec;
    } else {
	printf "/* UNKNOWN RECORD reclen= %d rec= %4d diff= %d */\n", myreclen, mycurrrec, mycurrrec-prevdouble ;
    }
}
