#include "frgg.h"
extern DB *g_tdbp;
extern char g_boards[MAX_BOARDS][BFNAMELEN+2];
extern int g_nboards;

struct dict_t **
new_postinglist_bucket(size_t size)
{
	struct dict_t **bucket = calloc(size, sizeof(struct dict_t *));
	if (bucket == NULL) 
		ERROR("calloc bucket failed");
	return bucket;
}


struct postinglist *
new_postinglist(size_t size)
{
	struct postinglist *p = malloc(sizeof(struct postinglist));
	if (p != NULL) {
		p->docs = malloc(size * sizeof(*p->docs));
		p->tf = malloc(size * sizeof(*p->tf));
		if (p->docs == NULL || p->tf == NULL) {
			ERROR("calloc p->docs or p->tf failed");
			return NULL;
		}
		p->size = size;
		p->freq = 0;
	} else {
		ERROR("malloc error");
	}
	return p;
}


/**
 * if term not in dict, add it
 */
struct postinglist *
get_postinglist(struct dict_t **bucket, size_t size, char *term)
{
	char *t = term;
	/* convert English word to lowercase */
	if (isalpha(*t)) {
		while (*t) {
			*t = tolower(*t);
			t++;
		}
	}
	unsigned int h = hash(term) % size;
	struct dict_t *entry = bucket[h];
	while (entry != NULL) {
		//DEBUG2("%s, %s", term, entry->term);
		if (strlen(term) == entry->term_size && 
                strncmp(term, entry->term, entry->term_size) == 0) {
			return entry->p;
        }
		entry = entry->next;
	}
	if (entry == NULL) {
		entry = malloc(sizeof(struct dict_t));
		if (entry == NULL) {
			ERROR("malloc failed");
			return NULL;
		}
		/* g_nterms++; */
		entry->term = strdup(term);
        entry->term_size = strlen(term);
		entry->p = new_postinglist(INIT_POSTINGLIST_SIZE);
		entry->next = bucket[h];
		bucket[h] = entry;
	}
	return entry->p;
}


int
double_postinglist(struct postinglist *p)
{
    assert(p->docs && p->tf);
	p->docs = realloc(p->docs, 2 * sizeof(*p->docs) * p->size);
	p->tf = realloc(p->tf, 2 * sizeof(*p->tf) * p->size);
	
	if (p->docs == NULL || p->tf == NULL) {
		ERROR("Oh shit! realloc failed");
		return -1;
	}
	p->size *= 2;
	return 0;
}


int
full_postinglist(struct postinglist *p)
{
	return p->freq >= p->size;
}


void
addto_postinglist(struct postinglist *p, unsigned int id)
{
    assert(p->freq < p->size && p->freq >= 0);
	if (p->freq > 0 && p->docs[p->freq - 1] == id) {
		p->tf[p->freq - 1]++;
	} else {
		p->docs[p->freq] = id;
		p->tf[p->freq++] = 1;
	}
}

/*
 * snappy compress.  return compress length 
 */
size_t compress_index_data(struct postinglist *p, char **pdatabuf, 
                            size_t *pdatabuf_size) 
{
    size_t init_size = sizeof(p->freq)  
        + sizeof(*(p->docs)) * p->freq + sizeof(*(p->tf)) * p->freq;
    char *postinglist_buf = (char*) malloc(init_size);
    if (postinglist_buf == NULL) {
        ERROR1("malloc fail size=%d\n", init_size);
        return 0;
    }
    char *cur = postinglist_buf;
    int i;
    memcpy(cur, &p->freq, sizeof(p->freq));
    cur += sizeof(p->freq);
    memcpy(cur, p->docs, sizeof(*p->docs) * p->freq); 
    cur += sizeof(*p->docs) * p->freq;
    memcpy(cur, p->tf, sizeof(*p->tf) * p->freq);
    
    size_t max_length;
    max_length = snappy_max_compressed_length(init_size);
    if (max_length > *pdatabuf_size) {
        *pdatabuf = realloc(*pdatabuf, max_length + 1);
        if (*pdatabuf == NULL) {
            ERROR("Oh shit realloc fail\n");
            return 0;
        }
        *pdatabuf_size = max_length;
    }
    
    int result;
    if ((result = snappy_compress(postinglist_buf, init_size, 
                        *pdatabuf, pdatabuf_size)) != SNAPPY_OK) {
        switch (result) {
            case SNAPPY_INVALID_INPUT:
                ERROR("snappy invalid input.\n");
                break;
            case SNAPPY_BUFFER_TOO_SMALL:
                ERROR("snappy output buffer too small\n");
                break;
            default:
                ERROR("snappy unknow error");
        }
        *pdatabuf_size = 0;
    }
    free(postinglist_buf);
    return *pdatabuf_size;
}

/*
 * snappy uncompress 
 */
int uncompress_index_data(char *input, size_t input_size, struct postinglist *p)
{
    size_t output_size;
    int result;

    if ((result=snappy_uncompressed_length(input, input_size, &output_size)) != SNAPPY_OK) {
         switch (result) {
            case SNAPPY_INVALID_INPUT:
                ERROR("snappy invalid input.\n");
                break;
            case SNAPPY_BUFFER_TOO_SMALL:
                ERROR("snappy output buffer too small\n");
                break;
            default:
                ERROR("snappy unknow error");
        }
        return -1;
    }
    char *output = (char*)malloc(output_size);
    if (snappy_uncompress(input, input_size, output, &output_size) != SNAPPY_OK) {
        ERROR("snappy uncompress fail\n");
        goto fail;
    }
    char *cur = output;
    memcpy(&p->freq, cur, sizeof(p->freq));

    if (p->freq > p->size) {
        if (p->size > 0) {
            p->docs = realloc(p->docs, p->freq * sizeof(*p->docs));
            p->tf = realloc(p->tf, p->freq * sizeof(*p->tf)); 
        } else {
            p->docs = calloc(p->freq * sizeof(*p->docs) + 1, 1);
            p->tf = calloc(p->freq * sizeof(*p->tf) + 1, 1); 
        }
        p->size = p->freq;
    }
    if (p->docs == NULL || p->tf == NULL) {
		ERROR("calloc p->docs or p->tf failed");
        p->freq = 0;
        goto fail;
    }

    cur += sizeof(p->freq);
    memcpy(p->docs, cur, sizeof(*p->docs) * p->freq);
    cur += sizeof(*p->docs) * p->freq;
    memcpy(p->tf, cur, sizeof(*p->tf) * p->freq);
    
    free(output);
    return 0;

fail:
    free(output);
    return -1;
} 

/**
 * varible byte codes
 */
int
vb_encode(unsigned int n, unsigned char *bytes)
{
	int i;
	int largest_bit = 0;
	for (i = 0; i < sizeof(unsigned int) * 8; ++i) 
		largest_bit = ((1 << i) & n) ? i : largest_bit;
	
	int nbyte = largest_bit / 7 + 1;
	i = nbyte;
	while (n) {
		bytes[--i] = (unsigned char)(n % 128);
		n /= 128;
	}
	/* set the first bit of last byte to 1 */
	bytes[nbyte - 1] |= 0x80;
	return nbyte;
}



/**
 * pack data from postinglist into buf (marshalling)
 * do compressing using varible byte encoding
 */
size_t
pack_index_data(struct postinglist *p, char *buf)
{
	int i;
	size_t buflen = 0;
	
	memcpy(buf, &(p->freq), sizeof(p->freq));
	buflen += sizeof(p->freq);

	if (p->freq > 0) {
		buflen += vb_encode(p->docs[0], (unsigned char *)(buf + buflen));
		buflen += vb_encode(p->tf[0], (unsigned char *)(buf + buflen));
	}
	
	for (i = 1; i < p->freq; i++) {
		buflen += vb_encode(p->docs[i] - p->docs[i - 1], (unsigned char *)(buf + buflen));
		buflen += vb_encode(p->tf[i], (unsigned char *)(buf + buflen));
	}
	return buflen;
}


int
write_index_file(struct dict_t **dict, size_t size, const char *indexfile)
{
	DB *dbp;
	DBT key, value;
	struct dict_t *ent, *tmp;
	int ret, i, result = -1;
	char *databuf;
    size_t databuf_size;

	/* Initialize the structure. This
	 * database is not opened in an environment, 
	 * so the environment pointer is NULL. */
	ret = db_create(&dbp, NULL, 0);
	if (ret != 0) {
		ERROR("create db hanldle failed");
		goto RETURN;
	}

	if (dbopen(dbp, indexfile, 1) != 0) {
		ERROR1("open db %s failed", indexfile);
		goto RETURN;
	}
    
	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));
    
    databuf_size = MAX_POSTLING_MEMORY_SIZE;
	databuf = (char*)malloc(databuf_size);
	if (databuf == NULL) {
		ERROR("malloc failed");
		goto CLEAN_DB;
	}
	/* tranverse entries */
	for (i = 0; i < size; ++i) {
		ent = dict[i];
		while (ent != NULL) {
			/* write_to_db */
			key.size = strlen(ent->term) + 1;
			key.data = ent->term;
			//value.size = pack_index_data(ent->p, databuf);
			value.size = compress_index_data(ent->p, &databuf, &databuf_size);
			value.data = databuf;
			dbp->put(dbp, NULL, &key, &value, 0);
            value.size = 1;
            g_tdbp->put(g_tdbp, NULL, &key, &value, 0);

			free(ent->p->docs);
			free(ent->p->tf);
			free(ent->p);
			tmp = ent;
			ent = ent->next;
			free(tmp);
		}
        dict[i] = NULL;
	}
	free(databuf);
	result = 0;
CLEAN_DB:	
	if (dbp != NULL)
		dbp->close(dbp, 0);
RETURN:	
	return result;
}

/* merge postinglist p, pn to p
 */
int merge_postinglist(struct postinglist *p, struct postinglist *pn, struct postinglist *pret)
{
    int idx1, idx2;
    pret->freq = 0;
    idx1 = idx2 = 0; 

    while(idx1 < p->freq || idx2 < pn->freq) {
        if (pret->freq >= pret->size) {
            if (pret->size == 0) {
                pret->size = INIT_POSTINGLIST_SIZE;
                pret->docs = calloc(sizeof(*pret->docs) * pret->size, 1);
                pret->tf = calloc(sizeof(*pret->tf) * pret->size, 1);
            } else {
                pret->size *= 2;
                pret->docs = realloc(pret->docs, pret->size * sizeof(*pret->docs));
                pret->tf = realloc(pret->tf, pret->size * sizeof(*pret->tf));
            }
        }
        if (idx1 < p->freq && idx2 < pn->freq 
                && p->docs[idx1] == pn->docs[idx2]) {
            pret->docs[pret->freq] = p->docs[idx1];      
            pret->tf[pret->freq] = p->tf[idx1] + pn->tf[idx2];
            pret->freq++;
            idx1++;
            idx2++;
        } else if (idx2 >= pn->freq || (idx1 < p->freq && p->docs[idx1] < pn->docs[idx2]) ) {
            pret->docs[pret->freq] = p->docs[idx1];  
            pret->tf[pret->freq] = p->tf[idx1];
            pret->freq++;
            idx1++;
        } else  {
            pret->docs[pret->freq] = pn->docs[idx2];
            pret->tf[pret->freq] = pn->tf[idx2];
            pret->freq++;
            idx2++;
        }
    }

    return 0;
}

int free_postinglist(struct postinglist *p)
{
    if (p->docs) free(p->docs);
    if (p->tf) free(p->tf);
    p->docs = p->tf = NULL;
    return 0;
}

/* 
 * merge index
 */
int merge_index(int type)
{
    DB *dbp[MAX_BOARDS], *findex_dbp;
    DBC *term_cursor;
    DBT term, val;
    struct postinglist p, pn, ptm; 
    char findexfile[PATH_MAX], indexfile[PATH_MAX];
    int i;
    int result = -1;
    memset(dbp, 0, sizeof(dbp));
    for(i = 0; i < g_nboards; i++) {
        if (db_create(&dbp[i], NULL, 0)) {
            ERROR("create db handle failed");
            continue;
        }
        if (type == BOARD)   {
            set_brdindex_file(indexfile, g_boards[i]);
        } else {
            set_annindex_file(indexfile, g_boards[i]);
        }
        if (dbopen(dbp[i], indexfile, 0) != 0) {
            ERROR1("open db file %s fail", indexfile);
        }
    }
    if (db_create(&findex_dbp, NULL, 0)) {
        ERROR("create final index db handler failed");
        goto CLEAN_BOARD_DB;
    }
    if (type == BOARD) {
        snprintf(findexfile, sizeof(findexfile), "%s", BOARDS_INDEX);
    } else  {
        snprintf(findexfile, sizeof(findexfile), "%s", ANN_INDEX);
    }
    if (dbopen(findex_dbp, findexfile, 1) != 0) {
        ERROR1("open db file %s fail\n", findexfile); 
        goto CLEAN_INDEX_DB;
    }
    g_tdbp->cursor(g_tdbp, NULL, &term_cursor, 0);

    int ret;
    size_t databuf_size;
    char *databuf;
    struct postinglist pswap;
    memset(&p, 0, sizeof(p));
    memset(&pn, 0, sizeof(pn));
    memset(&ptm, 0, sizeof(ptm));
    databuf_size = MAX_POSTLING_MEMORY_SIZE;
    databuf = (char*)malloc(databuf_size);
    
    fprintf(stderr, "merging index\n");
    memset(&term, 0, sizeof(term));
    memset(&val, 0, sizeof(val));
    
    int pcnt = 0;
    while((ret = term_cursor->get(term_cursor, &term, &val, DB_NEXT)) == 0) {
        p.freq = 0;
        for(i = 0; i < g_nboards; i++)  {
            ret = dbp[i]->get(dbp[i], NULL, &term, &val, 0);
            if (!val.data) continue;
            uncompress_index_data((char*)val.data, val.size, &pn);
            if (pn.freq == 0) continue;
            pn.size = pn.freq;
            merge_postinglist(&p, &pn, &ptm);
            pswap = p;
            p = ptm;
            ptm = pswap;
        }
        //write to final index file 
        val.size = compress_index_data(&p, &databuf, &databuf_size);
        val.data = databuf;
        findex_dbp->put(findex_dbp, NULL, &term, &val, 0);
    }
    term_cursor->close(term_cursor);
    free(databuf);
    free_postinglist(&p);
    free_postinglist(&pn);
    free_postinglist(&ptm);
    result = 0;
CLEAN_INDEX_DB:
    findex_dbp->close(findex_dbp, 0);
CLEAN_BOARD_DB:
    for(i = 0; i < g_nboards; i++) {
        dbp[i]->close(dbp[i], 0);
    }
    g_tdbp->close(g_tdbp, 0);
     
    return result;
}

/**
 *   open index db
 *   for each term:
 *	for doc,tf in term.postinglist:
 *	   W[doc] += sq(weight_d_t(tf))
 *   W[doc] = sqrt(W[doc])
 *   save W[doc] to file
 */
int
calc_doc_weight(int type)
{
	DB *dbp, *wdbp;
	DBC *cursorp;
	DBT key, value;
	int ret, i;
	double *weight, w;
	char indexfile[PATH_MAX], weightfile[PATH_MAX];
    char ndocsfile[PATH_MAX];
	FILE *fp;
	unsigned int ndocs;
    struct postinglist p;

	if (type == ANNOUNCE) {
        SET_NFILE(indexfile, ANN_INDEX);
        SET_NFILE(ndocsfile, ANN_NDOCS);
        SET_NFILE(weightfile, ANN_WEIGHT);
	} else {
        SET_NFILE(indexfile, BOARDS_INDEX);
        SET_NFILE(ndocsfile, BOARDS_NDOCS);
        SET_NFILE(weightfile, BOARDS_WEIGHT);
	}
    
    fp = fopen(ndocsfile, "r");
    if (fp == NULL) {
        ERROR1("open ndocs file %s fail", ndocsfile);
        return -1;
    }
    fscanf(fp, "%u", &ndocs); 
    fclose(fp);
    
    weight = (double*)calloc((ndocs+1) * sizeof(double), 1);
    if (weight == NULL) {
        ERROR("malloc weight fail\n");
        return -1;
    }
    fprintf(stderr, "calculating doc weight\n");

	/* Initialize the DB structure. */
	if ((ret = db_create(&dbp, NULL, 0)) != 0) {
		ERROR("create db hanldle failed");
		return -1;
	}
    if ((ret = db_create(&wdbp, NULL, 0)) != 0) {
		ERROR("create db hanldle failed");
        dbp->close(dbp, 0);
		return -1;
	}

	if (dbopen(dbp, indexfile, 0) != 0) {
		ERROR1("open db %s failed", indexfile);
        dbp->close(dbp, 0);
        wdbp->close(wdbp, 0);
		return -1;
	}
    if (dbopen(wdbp, weightfile, 1) != 0) {
		ERROR1("open db %s failed", weight);
        dbp->close(dbp, 0);
        wdbp->close(wdbp, 0);
        return -1;
    }

	/* Get a cursor */
	dbp->cursor(dbp, NULL, &cursorp, 0);

	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));
    memset(&p, 0, sizeof(p));

	/* Iterate over the database, retrieving each record in turn. */
	while ((ret = cursorp->get(cursorp, &key, &value, DB_NEXT)) == 0) {
        ret = uncompress_index_data(value.data, value.size, &p); 
        if (ret != 0) {
            ERROR("uncompress error");
            continue;
        }
        for(i = 0; i < p.freq; i++)  {
            w = weight_d_t(p.tf[i]);
            weight[p.docs[i]] += w * w;
        }
    }

    key.size = sizeof(i); 
    key.data = &i;
	for (i = 1; i <= ndocs; ++i) {
		weight[i] = sqrt(weight[i]);
        value.size = sizeof(double);
        value.data = &weight[i];

        wdbp->put(wdbp, NULL, &key, &value, 0);
	}
	free(weight);
    free_postinglist(&p);
	cursorp->close(cursorp);
    dbp->close(dbp, 0);
	return 0;
}
