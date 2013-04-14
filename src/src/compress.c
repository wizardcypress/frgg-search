#include "frgg.h"
    
static int 
dump_snappy_error(int result, int type)
{
    const char * ptr;
    if (type == 0)
        ptr = "compress";
    else
        ptr = "uncompress";
    
    switch (result) {
        case SNAPPY_INVALID_INPUT:
            ERROR1("%s snappy invalid input.", ptr);
            break;
        case SNAPPY_BUFFER_TOO_SMALL:
            ERROR1("%s snappy output buffer too small", ptr);
            break;
        default:
            ERROR("snappy unknow error");
    }
    return 0;
}


/*
 * snappy compress.  return compress length 
 */
size_t 
compress_index_data(struct postinglist *p, char **pdatabuf, 
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
        dump_snappy_error(result, 0);
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
    int result = -1;

    if ((result=snappy_uncompressed_length(input, input_size, &output_size)) != SNAPPY_OK) {
        dump_snappy_error(result, 1);
        goto fail;
    }
    char *output = (char*)malloc(output_size + 10);
    if ((result = snappy_uncompress(input, input_size, output, &output_size)) != SNAPPY_OK) {
        dump_snappy_error(result, 1);
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

