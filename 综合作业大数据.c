#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define WORD_MAX 128
#define WORD_CNT_MAX 2000000
#define HASH_SIZE 1048573      // 增大哈希表容量为质数，减少冲突
#define BIGRAM_CAND_LIMIT 100000

typedef struct DictNode {
    char *word;
    struct DictNode *next;
} DictNode;
DictNode *dict_table[HASH_SIZE];

unsigned hash_str(const char *s) {
    unsigned hash = 0;
    while (*s) hash = hash * 131 + (*s++);
    return hash % HASH_SIZE;
}
char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *d = (char *)malloc(len);
    if (d) memcpy(d, s, len);
    return d;
}
void insert_dict(const char *word) {
    unsigned h = hash_str(word);
    DictNode *node = (DictNode *)malloc(sizeof(DictNode));
    node->word = my_strdup(word);
    node->next = dict_table[h];
    dict_table[h] = node;
}
int in_dict(const char *word) {
    unsigned h = hash_str(word);
    for (DictNode *p = dict_table[h]; p; p = p->next)
        if (strcmp(p->word, word) == 0) return 1;
    return 0;
}

// 2-gram 哈希表
typedef struct BigramNode {
    char *second;
    struct BigramNode *next;
} BigramNode;
typedef struct BigramList {
    char *first;
    BigramNode *seconds;
    struct BigramList *next;
} BigramList;
BigramList *bigram_table[HASH_SIZE];
void insert_bigram(const char *first, const char *second) {
    unsigned h = hash_str(first);
    BigramList *p;
    for (p = bigram_table[h]; p; p = p->next)
        if (strcmp(p->first, first) == 0) break;
    if (!p) {
        p = (BigramList *)malloc(sizeof(BigramList));
        p->first = my_strdup(first);
        p->seconds = NULL;
        p->next = bigram_table[h];
        bigram_table[h] = p;
    }
    for (BigramNode *q = p->seconds; q; q = q->next)
        if (strcmp(q->second, second) == 0) return;
    BigramNode *newnode = (BigramNode *)malloc(sizeof(BigramNode));
    newnode->second = my_strdup(second);
    newnode->next = p->seconds;
    p->seconds = newnode;
}
int get_bigrams(const char *first, char **arr, int *count) {
    unsigned h = hash_str(first);
    *count = 0;
    BigramList *p = bigram_table[h];
    while (p) {
        if (strcmp(p->first, first) == 0) {
            BigramNode *q = p->seconds;
            while (q && *count < BIGRAM_CAND_LIMIT) {
                arr[(*count)++] = q->second;
                q = q->next;
            }
            if (*count >= BIGRAM_CAND_LIMIT) break;
            return 1;
        }
        p = p->next;
    }
    return 0;
}

typedef struct {
    char word[WORD_MAX];
    int in_dict;
    int is_error;
    int seg_id;
} WordInfo;
WordInfo *words = NULL;
int word_cnt = 0;

typedef struct { int idx; } ErrorAppear;
ErrorAppear *errorAppear = NULL;
int errorAppearCnt = 0;
unsigned *appeared = NULL;

unsigned err_hash(const char *w) {
    unsigned h = 0;
    while (*w) h = h*131+(*w++);
    return h % HASH_SIZE;
}
int error_word_appeared(const char *w) {
    unsigned h = err_hash(w);
    if (appeared[h]) return 1;
    appeared[h] = 1;
    return 0;
}

void str_tolower(char *s) { for (; *s; ++s) *s = tolower(*s); }

// 动态扩容words和errorAppear
void ensure_words_capacity(int needed) {
    static int cap = 0;
    if (needed < cap) return;
    cap = cap == 0 ? 4096 : cap * 2;
    if (cap < needed) cap = needed + 1024;
    words = (WordInfo*)realloc(words, sizeof(WordInfo) * cap);
    errorAppear = (ErrorAppear*)realloc(errorAppear, sizeof(ErrorAppear) * cap);
}

void load_dict(const char *filename) {
    FILE *fdict = fopen(filename, "r");
    if (!fdict) { puts("Can't open dict.txt!"); exit(1); }
    char buf[WORD_MAX];
    while (fgets(buf, WORD_MAX, fdict)) {
        buf[strcspn(buf, "\r\n")] = 0;
        str_tolower(buf);
        insert_dict(buf);
    }
    fclose(fdict);
}
void process_input(const char *filename) {
    FILE *fin = fopen(filename, "r");
    if (!fin) { puts("Can't open in.txt!"); exit(1); }
    int c, i = 0, seg_id = 0;
    char wbuf[WORD_MAX];
    while ((c = fgetc(fin)) != EOF) {
        if (isalpha(c)) {
            if (i < WORD_MAX-1) wbuf[i++] = tolower(c);
        } else if (c == '\'') {
            if (i > 0) {
                wbuf[i] = 0;
                ensure_words_capacity(word_cnt+1);
                strcpy(words[word_cnt].word, wbuf);
                words[word_cnt].in_dict = in_dict(wbuf);
                words[word_cnt].is_error = !words[word_cnt].in_dict;
                words[word_cnt].seg_id = seg_id;
                word_cnt++;
                i = 0;
            }
        } else if (c == ' ' || c == '\t') {
            if (i > 0) {
                wbuf[i] = 0;
                ensure_words_capacity(word_cnt+1);
                strcpy(words[word_cnt].word, wbuf);
                words[word_cnt].in_dict = in_dict(wbuf);
                words[word_cnt].is_error = !words[word_cnt].in_dict;
                words[word_cnt].seg_id = seg_id;
                word_cnt++;
                i = 0;
            }
        } else {
            if (i > 0) {
                wbuf[i] = 0;
                ensure_words_capacity(word_cnt+1);
                strcpy(words[word_cnt].word, wbuf);
                words[word_cnt].in_dict = in_dict(wbuf);
                words[word_cnt].is_error = !words[word_cnt].in_dict;
                words[word_cnt].seg_id = seg_id;
                word_cnt++;
                i = 0;
            }
            seg_id++;
        }
        if (word_cnt >= WORD_CNT_MAX-2) break;
    }
    if (i > 0) {
        wbuf[i] = 0;
        ensure_words_capacity(word_cnt+1);
        strcpy(words[word_cnt].word, wbuf);
        words[word_cnt].in_dict = in_dict(wbuf);
        words[word_cnt].is_error = !words[word_cnt].in_dict;
        words[word_cnt].seg_id = seg_id;
        word_cnt++;
    }
    fclose(fin);
}
void build_bigrams() {
    for (int i = 1; i < word_cnt; ++i) {
        if (words[i].seg_id == words[i-1].seg_id &&
            words[i-1].in_dict && words[i].in_dict) {
            insert_bigram(words[i-1].word, words[i].word);
        }
    }
}
void collect_error_words() {
    for (int i = 1; i < word_cnt; ++i) {
        if (words[i].seg_id == words[i-1].seg_id &&
            words[i-1].in_dict && words[i].is_error) {
            if (!error_word_appeared(words[i].word)) {
                ensure_words_capacity(errorAppearCnt+1);
                errorAppear[errorAppearCnt++].idx = i;
            }
        }
    }
}
int min3(int a, int b, int c) {
    int min = a < b ? a : b;
    return min < c ? min : c;
}
// 用静态数组提升效率，最大单词128足够
int editdistDP(const char *str1, const char *str2) {
    static int dp[WORD_MAX+1][WORD_MAX+1];
    int len1 = strlen(str1), len2 = strlen(str2);
    for (int i=0; i<=len1; ++i)
        for (int j=0; j<=len2; ++j)
            if (i==0) dp[i][j] = j;
            else if (j==0) dp[i][j] = i;
            else if (str1[i-1]==str2[j-1]) dp[i][j] = dp[i-1][j-1];
            else dp[i][j] = 1 + min3(dp[i][j-1], dp[i-1][j], dp[i-1][j-1]);
    return dp[len1][len2];
}
int cmpstr(const void *a, const void *b) { return strcmp(*(char**)a, *(char**)b); }
void suggest_correction(int idx) {
    char *prefix = words[idx-1].word;
    char *err = words[idx].word;
    char *seconds[BIGRAM_CAND_LIMIT];
    int scnt = 0;
    if (!get_bigrams(prefix, seconds, &scnt) || scnt == 0) {
        printf("%s: %s -> No suggestion\n", prefix, err);
        return;
    }
    if (scnt > BIGRAM_CAND_LIMIT) scnt = BIGRAM_CAND_LIMIT;
    int mindist = 10000;
    for (int i = 0; i < scnt; ++i) {
        int d = editdistDP(err, seconds[i]);
        if (d < mindist) mindist = d;
    }
    char *cands[BIGRAM_CAND_LIMIT];
    int candn = 0;
    for (int i = 0; i < scnt; ++i) {
        int d = editdistDP(err, seconds[i]);
        if (d == mindist) {
            int found = 0;
            for (int k=0; k<candn; ++k)
                if (strcmp(cands[k], seconds[i])==0) {found=1; break;}
            if (!found && candn < BIGRAM_CAND_LIMIT) cands[candn++] = seconds[i];
        }
    }
    if (candn > 1) qsort(cands, candn, sizeof(char*), cmpstr);
    printf("%s: %s -> ", prefix, err);
    for (int i = 0; i < candn; ++i) {
        if (i) printf(",");
        printf("%s", cands[i]);
    }
    printf("\n");
}

int main() {
    appeared = (unsigned*)calloc(HASH_SIZE, sizeof(unsigned));
    words = (WordInfo*)malloc(sizeof(WordInfo) * 4096);
    errorAppear = (ErrorAppear*)malloc(sizeof(ErrorAppear) * 4096);

    load_dict("dict.txt");
    process_input("in.txt");
    build_bigrams();
    collect_error_words();
    for (int i = 0; i < errorAppearCnt; ++i) {
        int idx = errorAppear[i].idx;
        suggest_correction(idx);
    }
    // 内存释放
    free(words); 
    free(errorAppear);
    free(appeared);
    // dict_table和bigram_table如果需极致释放可递归free，这里略
    return 0;
}
