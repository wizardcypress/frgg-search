#include "frgg.h"

char *g_text;
extern off_t g_pos;
extern off_t g_len;
char g_boards[MAX_BOARDS][BFNAMELEN+2];
int g_nboards;
DB *g_tdbp;

int index_board(const char *bname, DB *dbp, DBT *key, DBT *data, unsigned int *pdocid,
                struct dict_t ** bucket, size_t size) 
{
	char dirfile[PATH_MAX], indexfile[PATH_MAX];
    char *word;
	char filepath[PATH_MAX]; /* article file path */
	char filename[20], postpath[PATH_MAX];
	char cachedir[PATH_MAX];
	char cachefile[PATH_MAX];
    FILE *filelist;
	gzFile cachefp;
	int gzerr;
	struct postinglist *p;

	setboardfile(dirfile, bname, bname);
	set_brdindex_file(indexfile, bname);
    
	if (!(filelist = fopen(dirfile, "r"))) {
		ERROR1("open file %s failed", dirfile);
        return -1;
	}

    /* ensure the cache directory exists */
	setcachepath(cachedir, bname);
	f_mkdir(cachedir, 0755);

	while (fgets(filename, sizeof(filename), filelist)) {
		filename[strlen(filename) - 1] = '\0';
	
		setboardfile(filepath, bname, filename);
		ansi_filter(filepath);
        
        snprintf(postpath, sizeof(postpath), "%s/%s", bname, filename);
        data->size = strlen(postpath) + 1;
		data->data = postpath;
			
		if (g_len != 0) {
			fprintf(stderr, "%d %s indexing %s\n", *pdocid, bname, filename);
			/* save to cache file */
			setcachefile(cachefile, bname, filename);
			cachefp = gzopen(cachefile, "wb");
			if (cachefp != NULL) {
				if (gzwrite(cachefp, g_text, g_len) != g_len) 
					ERROR(gzerror(cachefp, &gzerr));
				gzclose(cachefp);
			}
			
			g_pos = 0;
			while ((word = next_token())) {
                p = get_postinglist(bucket, size, word);
				addto_postinglist(p, *pdocid);
				if (p->freq >= p->size) /* is full */
					double_postinglist(p);
                free(word);
			}
			
			/* write_docid2path */
			dbp->put(dbp, NULL, key, data, 0);
			(*pdocid)++;
		}
	}

    write_index_file(bucket, size, indexfile);

    return 0;
}

// single pass in-memory index
/**1 output_file = new file()
  2 dict =  new hash()
  3 while (free memory available)
  4 do token = next_token()
  5     if token not in dict
  6  	   postinglist = addtodict(dict, token)
  7     else postinglist = getpostinglist(dict, token)
  8     if full(postinglist)
  9   	   postinglist = doublepostinglist(dict, token)
  10    addtopostinglist(postinglist, docid(token))
  11 sorted_terms = sortterm(dict)	// for merge purpose 
 *12 writeblock(sorted_terms, dict, output_file)
 */
int
build_board_index()
{
    char docid2path[PATH_MAX];
	char ndocsfile[PATH_MAX], bname[PATH_MAX];
    char termfile[PATH_MAX];
	DB *dbp;
	DBT key, data;
	int ret;
	int result = -1;
	FILE  *fp;
	unsigned int docid = 1;

	/* Initialize the  DB structure.*/
	ret = db_create(&dbp, NULL, 0);
    ret |= db_create(&g_tdbp, NULL, 0);
	if (ret != 0) {
		ERROR("create db hanldle failed");
		goto RETURN;
	}

	g_text = malloc(MAX_FILE_SIZE);
	if (g_text == NULL) {
		ERROR("malloc failed");
		goto CLEAN_DB;
	}
    
    snprintf(docid2path, sizeof(docid2path), "%s", BOARDS_DOCID2PATH); 
    snprintf(termfile, sizeof(termfile), "%s", ALL_TERMS);
    if (dbopen(dbp, docid2path, 1) != 0) {
		ERROR1("open db %s failed", docid2path);
		goto CLEAN_DB;
	}
    if (dbopen(g_tdbp, termfile, 1) != 0) {
		ERROR1("open db %s failed", termfile);
		goto CLEAN_DB;
    }
	
	size_t size = MAX_POSTING_HASHSIZE;	/* TODO: define this constant */
	struct dict_t **bucket = new_postinglist_bucket(size + 10);
	if (bucket == NULL) {
		ERROR1("new_dict size=%u failed", size);
		goto CLEAN_DB;
	}

	/* Zero out the DBTs before using them. */
	memset(&key, 0, sizeof(DBT));
	memset(&data, 0, sizeof(DBT));
	
	key.size = sizeof(unsigned int);
	key.data = &docid;
    
    FILE *boardlist = fopen(BOARDS_LIST, "r");
    if (boardlist == NULL) {
        ERROR2("open %s fail: %s", BOARDS_LIST, strerror(errno));
        goto CLEAN_MEM;
    }
    
    g_nboards = 0;
    while(fgets(bname, sizeof(bname), boardlist)) {
        if (bname[strlen(bname)-1] == '\n')
            bname[strlen(bname)-1] = '\0';
        if (bname[0] == '\0') continue;
        strncpy(g_boards[g_nboards], bname, sizeof(g_boards[0]));
        g_nboards++;
        index_board(bname, dbp, &key, &data, &docid, bucket, size);
    }

    snprintf(ndocsfile, sizeof(ndocsfile), "%s", BOARDS_NDOCS);
	fp = fopen(ndocsfile, "w");
	if (fp == NULL) {
		ERROR1("fopen %s failed", ndocsfile);
        goto CLEAN_MEM;
	}
	fprintf(fp, "%u", docid - 1);
	fclose(fp);

	/* it's ok */
	result = 0;
CLEAN_MEM:
	free(bucket);
CLEAN_DB:
	if (dbp != NULL)
		dbp->close(dbp, 0);
RETURN:
	free(g_text);	
	return result;
}


int main(int argc, char *argv[])
{
	chdir(FRGGHOME);
    /* setup signal hanlders */
    setup_signal_handlers();

	init_segment();		/* load dict */
	build_board_index();
    merge_index(BOARD);
    calc_doc_weight(BOARD);
	cleanup_segment();
	return 0;
}
