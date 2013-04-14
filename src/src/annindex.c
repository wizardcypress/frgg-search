#include "frgg.h"

char *g_text;
extern off_t g_pos;
extern off_t g_len;

/* global varibles for Announce indexing */
unsigned int g_docid = 1;
unsigned int g_size = MAX_POSTING_HASHSIZE;

struct dict_t **g_bucket; 
char g_boards[MAX_BOARDS][BFNAMELEN+2];
int g_nboards;
DB *g_dbp, *g_tdbp;
DBT g_key, g_data;


int
index_file(char *filename)
{
	char *word;
	struct postinglist *p;

	
	ansi_filter(filename);
	if (g_len != 0) {
		fprintf(stderr, "%d indexing %s\n", g_docid, filename);
		/* @TODO: save to cache file */
		g_pos = 0;
		while ((word = next_token())) {
			//DEBUG1("%s", word);
			p = get_postinglist(g_bucket, g_size, word);
			addto_postinglist(p, g_docid);
			if (p->freq >= p->size) /* is full */
				double_postinglist(p);
            free(word);
		}
		g_data.size = strlen(filename) + 1;
		g_data.data = filename;		
		/* write_docid2path */
		g_dbp->put(g_dbp, NULL, &g_key, &g_data, 0);
		g_docid++;
	}
	return 0;
}


void
ann_traverse(char *directory, int (*process_func)(char *))
{
	char dotdir[PATH_MAX], sdir[PATH_MAX], filename[PATH_MAX];
	struct annheader header;
	FILE *fpdir;

	snprintf(dotdir, sizeof(dotdir), "%s/.DIR", directory);
	if ((fpdir = fopen(dotdir, "r")) == NULL)      /* no .DIR found */
		return;

	while (fread(&header, 1, sizeof(header), fpdir) == sizeof(header)) {
		//　目录/#或留言本#/, 递归 
		if ((header.flag & ANN_DIR) /* || (header.flag & ANN_GUESTBOOK) */) {
			/* TODO: save title to stack */
			snprintf(sdir, sizeof(sdir), "%s/%s", directory, header.filename);
			ann_traverse(sdir, process_func);
		} else {
			snprintf(filename, sizeof(filename), "%s/%s", directory, header.filename);
			//fprintf(stderr, "indexing %u %s %s\n ", g_docid, filename, header.title);
			process_func(filename);
		}
	}
	fclose(fpdir);
}


/**
 * @TODO
 */
int
build_ann_index()
{
	char docid2path[PATH_MAX], indexfile[PATH_MAX];
	char annpath[PATH_MAX];
	char ndocsfile[PATH_MAX];
    char termfile[PATH_MAX];
	int ret;
	int result = -1;
	FILE *fp, *boardlist;
    char bname[PATH_MAX];
    
    SET_NFILE(docid2path, ANN_DOCID2PATH);
    SET_NFILE(termfile, ALL_TERMS);

    if ((boardlist = fopen(BOARDS_LIST, "r")) == NULL) {
        ERROR("open board list fail");
        return -1;
    }

	/* Initialize the DB structure.*/
	if ((ret = db_create(&g_dbp, NULL, 0)) != 0) {
		ERROR("create db hanldle failed");
		goto RETURN;
	}
    if ((ret = db_create(&g_tdbp, NULL, 0)) != 0) {
        ERROR("create g_tdbp handler failed");
        g_dbp->close(g_dbp, 0);
        goto RETURN;
    }
    
	if (dbopen(g_dbp, docid2path, 1) != 0) {
		ERROR1("open db %s failed", docid2path);
		goto RETURN;		
	}
    if (dbopen(g_tdbp, termfile, 1) != 0) {
        ERROR1("open db %s failed", termfile);
        goto CLEAN_DB;
    }

	g_bucket = new_postinglist_bucket(g_size);
	if (g_bucket == NULL) {
		ERROR1("new_postinglist_bucket size=%u failed", g_size);
		goto CLEAN_DB;
	}

	g_text = malloc(MAX_FILE_SIZE);
	if (g_text == NULL) {
		ERROR("malloc failed");
		goto CLEAN_MEM;
	}

	/* Zero out the DBTs before using them. */
	memset(&g_key, 0, sizeof(DBT));
	memset(&g_data, 0, sizeof(DBT));
	
	g_key.size = sizeof(unsigned int);
	g_key.data = &g_docid;
    
    g_nboards = 0;
    while(fgets(bname, sizeof(bname), boardlist)) {
        if (bname[strlen(bname)-1] == '\n') 
            bname[strlen(bname)-1] = '\0';
        
        SET_NFILE(g_boards[g_nboards], bname);
        g_nboards++;
        snprintf(annpath, sizeof(annpath), "data/0Announce/%s", bname);
	    set_annindex_file(indexfile, bname);

	    /* TODO: cache, ensure the cache directory exists */
	    ann_traverse(annpath, index_file);
	    write_index_file(g_bucket, g_size, indexfile);
    }
    
    SET_NFILE(ndocsfile, ANN_NDOCS);
	fp = fopen(ndocsfile, "w");
	if (fp == NULL) {
		ERROR1("fopen %s failed", ndocsfile);
		goto CLEAN;
	}
	fprintf(fp, "%u", g_docid - 1);
	fclose(fp);

	/* it's ok */
	result = 0;
CLEAN:	
	free(g_text);	
CLEAN_MEM:
	free(g_bucket);
CLEAN_DB:
	if (g_dbp != NULL)
		g_dbp->close(g_dbp, 0);
RETURN:
    fclose(boardlist);
	return result;
}



int main(int argc, char *argv[])
{
	chdir(FRGGHOME);
    /* setup signal hanlders */
    setup_signal_handlers();

	init_segment();		/* load dict */
	build_ann_index();
    merge_index(ANNOUNCE);
	calc_doc_weight(ANNOUNCE);
	cleanup_segment();
	return 0;
}
