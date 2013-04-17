db_include_path = /usr/local/BerkeleyDB.5.3/include/
db_library_path = /usr/local/BerkeleyDB.5.3/lib/

CC=cc
CFLAGS = -Wall -g -D_GNU_SOURCE
include_opt = -I../include -I$(db_include_path)
library_opt = -L$(db_library_path) -ldb -lm -lz -lsnappy

all: buildbrdindex buildannindex search fetch_filelist get_files cache filelist

update: buildbrdindex buildannindex search fetch_filelist get_files cache filelist
	cp search ../../www/cgi-bin/;
	cp buildbrdindex ../../bin/;
	cp buildannindex ../../bin/;
	cp fetch_filelist ../../bin/;
	cp get_files ../../bin/;
	cp cache ../../www/cgi-bin/;

filelist: filelist.o log.o
	${CC} ${CFLAGS} -o $@ $^ ${include_opt}

fetch_filelist: fetch_filelist.o log.o fileop.o
	${CC} $(CFLAGS) -o fetch_filelist $^ $(include_opt) -lcurl

get_files: get_files.o log.o 
	${CC} $(CFLAGS) -o get_files $^ $(include_opt) -lcurl

buildbrdindex: brdindex.o index.o segment.o dict.o ansifilter.o log.o db.o formula.o fileop.o string.o debug.o compress.o
	${CC} $(CFLAGS) -o buildbrdindex  $^ $(library_opt)

buildannindex: annindex.o index.o segment.o dict.o ansifilter.o log.o db.o formula.o fileop.o string.o debug.o  compress.o
	${CC} $(CFLAGS) -o buildannindex $^ $(library_opt)

search: cgi.o formula.o log.o db.o segment.o dict.o search.o ansifilter.o string.o debug.o compress.o
	${CC} $(CFLAGS) -o search  $^  $(include_opt) $(library_opt)

cache: showcache.c
	${CC} $(CACHE) -o cache showcache.c $(include_opt) $(library_opt)

%.o: %.c
	${CC} ${CFLAGS} -c $< ${include_opt}

.PHONY: clean
clean:
	rm -f *.o core buildbrdindex buildannindex search segment fetch_filelist get_files cache
