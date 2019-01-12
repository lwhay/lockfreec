#include "internal.h"
#include "abstraction.h"
#include "advance/lf_memtrie.h"
//#define FILEPATH  "/home/hadoop/testjar_lubm/University.nt"
//#define FILEPATH  "/home/hadoop/testjar_lubm/University_rand.nt"
//#define FILEPATH  "/home/hadoop/testjar_lubm/dbpedia_all.nt"
//#define FILEPATH  "/home/hadoop/testjar_dbpedia/University_rand_longstr.nt"
#define FILEPATH  "d:/trie_data/University_rand_longstr.nt"
#define STRLEN_MAX  1024
#define CPU_COUNT_NUM 8
pthread_mutex_t readfile_mutex;
FILE *fp;

int fgets_str(char *str, FILE *read_file) {
    if (NULL == read_file) {
        printf("read file error!");
        exit(1);
    }
    char *rv_str;

    rv_str = fgets(str, STRLEN_MAX, read_file);
    if (rv_str != NULL) {
        //  printf("get str success:%s",rv_str);
        return 1;
    }
    return 0;
}

char **split(char *str, char split_label) {
    int i = 0;
    int label_num = 0, npos = 0;
    char **strlist = (char **) malloc(sizeof(char *) * 3);
    for (i = 0; i < strlen(str) && label_num < 3; i++) {
        if (str[i] == split_label) {
            strlist[label_num] = (char *) malloc(sizeof(char) * (i - npos + 1));
            memcpy(strlist[label_num], str + npos, (i - npos));
            strlist[label_num][i - npos] = '\0';
            label_num++;
            npos = i + 1;
        }
    }
    //printf("split over");
    return strlist;
}

void free_strlist(char **strlist) {
    char *str;
    int i = 0;
    for (i = 0; i < 3; i++) {
        str = strlist[i];
        free(str);
    }
}

void test_memtrie(void) {
    printf("\n"
           "MemTrie Tests\n"
           "==============\n");
    memtrie_test_internal_insert();
    //memtrie_test_internal_search();
    //    memtrie_test_internal_insert_and_search();
    //memtrie_test_internal_rapid_insert_and_search();
}

void read2startline(atom_t startline, FILE *read_file) {
    char str[1024];
    atom_t lines = startline;
    while (lines > 0 && fgets(str, STRLEN_MAX, read_file))
        lines--;
}

atom_t read_file_count(int cpu_count) {
    fp = fopen(FILEPATH, "r");
    if (NULL == fp) {
        printf("read file error!");
        exit(1);
    }
    atom_t lines = 0;
    char str[1024];
    while (fgets(str, STRLEN_MAX, fp) && strlen(str) > 1) {
        lines++;
    }
    //    if(lines % cpu_count)
    return lines / cpu_count;
    //    else
    //        return lines/(cpu_count - 1);
}

void close_readfile() {
    if (NULL != fp) {
        fclose(fp);
    }
}

void memtrie_test_internal_insert(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct trie_state *ts;
    struct trie_state_test *tst;
    atom_t data, thread, count, *per_thread_counters;
    struct validation_info vi = {1000000, 1000000};
    enum data_structure_validity dvs[2];
    internal_display_test_name("MemTrie insert...");
    cpu_count = abstraction_cpu_count();
    cpu_count = CPU_COUNT_NUM;
    //    cpu_count = 24;
    if (!(trie_new(&ts, TRIE_NEW_NODELIST_NUM))) {
        internal_display_test_name("MemTrie new over...");
        return;
    }
    atom_t tmp = ts->trieNodeList->nodelist_counter;
    printf("\nnodelist_num:%d\n", tmp);
    internal_display_test_name("MemTrie test  2...");
    tst = abstraction_malloc(sizeof(struct trie_state_test) * cpu_count);
    //init open file and read mutex;
    atom_t read_line = read_file_count(cpu_count);
    printf("\nread_lines:%u\n", read_line);
    pthread_mutex_init(&readfile_mutex, NULL);

    for (loop = 0; loop < cpu_count; loop++) {
        (tst + loop)->trie_state = ts;
        (tst + loop)->thread_num = loop;
        (tst + loop)->read_line = read_line;

    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, memtrie_test_internal_thread_simple_insert, tst + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    tmp = tst->trie_state->trieNodeList->nodelist_counter;
    printf("\nnodelist_num2:%d\n", tmp);
    abstraction_free(thread_handles);
    close_readfile();
    memtrie_test_internal_search(tst->trie_state);
    abstraction_free(tst);
    //test
    trie_state_free(ts);
    internal_display_test_name("trie_simple_insert over !");
    return;
}

thread_return_t CALLING_CONVENTION memtrie_test_internal_thread_simple_insert(void *state) {
    struct trie_state_test *tst;
    struct hash_data *data;
    atom_t insert_num = 0, error_num = 0, duplication = 0, readline = 0, maxline = 0;
    int strlength = 0, deal_num = 0;
    char **current_str, **str_list;

    assert(NULL != state);
    tst = (struct trie_state_test *) state;
    trie_use(tst->trie_state);
    FILE *read_file = fopen(FILEPATH, "r");
    if (NULL == read_file) {
        printf("open file error");
        return (thread_return_t) EXIT_FAILURE;
    }
    readline = (tst->read_line) * (tst->thread_num);
    maxline = (tst->read_line) * (tst->thread_num + 1);
    read2startline(readline, read_file);
    //    char* test = "abcdefghijklmn",*str;
    //    strlen = 14;
    struct timeval tpstart, tpend, deal_start, deal_end;
    double total_time = 0.0;
    char *str = (char *) malloc(sizeof(char) * STRLEN_MAX);
    gettimeofday(&tpstart, NULL);
    while (readline < maxline + 1 && fgets_str(str, read_file)) {
        //printf("start split");
        str_list = split(str, ' ');
        gettimeofday(&deal_start, NULL);
        int loop;
        for (loop = 0; loop < 3; loop++) {
            deal_num++;
            //            printf("\ndeal_num:%d\n",deal_num);
            strlength = strlen(str_list[loop]);
            if (strlength < 1) {
                continue;
            }
            //            printf("str_list item:%s",str_list[loop]);
            switch (lf_trie_insert_real_data(tst->trie_state, str_list[loop], strlength)) {
                case TRIE_INSERT_SUCCESS:
                    insert_num++;
                    break;
                case TRIE_INSERT_ERROR:
                    error_num++;
                    break;
                case TRIE_INSERT_DUPLICATION:
                    duplication++;
                default:
                    break;
            }
        }
        readline++;
        free_strlist(str_list);
        gettimeofday(&deal_end, NULL);
        total_time += 1000000 * (deal_end.tv_sec - deal_start.tv_sec) + deal_end.tv_usec - deal_start.tv_usec;
        //        if(deal_num % 100000 == 0)
        //        {
        //            printf("%u\t%u\t%lf\n",tst->thread_num,deal_num,total_time/1000000);
        //        }
    }
    total_time /= 1000000;
    fclose(read_file);
    //test
    //    atom_t tmp = tst->trie_state->trieNodeList->nodelist_counter;
    //    printf("\nnodelist_num:%d\n",tmp);
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    timeused /= 1000000;
    printf("\n%u\t%u\t%lf\t%lf\t%u\t%u\n\n", tst->thread_num, deal_num, timeused, total_time, insert_num, duplication);
    fflush(stdout);
    return ((thread_return_t) EXIT_SUCCESS);
}

void memtrie_test_internal_search(struct trie_state *ts) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct trie_state_test *tst;
    atom_t data, thread, count, *per_thread_counters;
    struct validation_info vi = {1000000, 1000000};
    enum data_structure_validity dvs[2];
    internal_display_test_name("MemTrie search...");
    cpu_count = abstraction_cpu_count();
    cpu_count = CPU_COUNT_NUM;
    //    cpu_count = 24;

    internal_display_test_name("MemTrie search test  2...");
    tst = abstraction_malloc(sizeof(struct trie_state_test) * cpu_count);
    //init open file and read mutex;
    atom_t read_line = read_file_count(cpu_count);
    printf("\nread_lines:%u\n", read_line);
    pthread_mutex_init(&readfile_mutex, NULL);

    for (loop = 0; loop < cpu_count; loop++) {
        (tst + loop)->trie_state = ts;
        (tst + loop)->thread_num = loop;
        (tst + loop)->read_line = read_line;

    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, memtrie_test_internal_thread_simple_search, tst + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    //tmp = tst->trie_state->trieNodeList->nodelist_counter;
    //printf("\nnodelist_num2:%d\n",tmp);
    abstraction_free(thread_handles);

    //test
    internal_display_test_name("trie_simple_search over !");
    return;

}

void memtrie_test_internal_thread_simple_search(void *state) {
    struct trie_state_test *tst;
    struct hash_data *data;
    atom_t search_num = 0, error_num = 0, duplication = 0, readline = 0, maxline = 0;
    int strlength = 0, deal_num = 0;
    char **current_str, **str_list;

    assert(NULL != state);
    tst = (struct trie_state_test *) state;
    trie_use(tst->trie_state);
    FILE *read_file = fopen(FILEPATH, "r");
    if (NULL == read_file) {
        printf("open file error");
        return (thread_return_t) EXIT_FAILURE;
    }
    readline = (tst->read_line) * (tst->thread_num);
    maxline = (tst->read_line) * (tst->thread_num + 1);
    read2startline(readline, read_file);
    //    char* test = "abcdefghijklmn",*str;
    //    strlen = 14;
    struct timeval tpstart, tpend, deal_start, deal_end;
    double total_time = 0.0;
    char *str = (char *) malloc(sizeof(char) * STRLEN_MAX);
    gettimeofday(&tpstart, NULL);
    while (readline < maxline + 1 && fgets_str(str, read_file)) {
        //printf("start split");
        str_list = split(str, ' ');
        gettimeofday(&deal_start, NULL);
        int loop;
        for (loop = 0; loop < 3; loop++) {
            deal_num++;
            //            printf("\ndeal_num:%d\n",deal_num);
            strlength = strlen(str_list[loop]);
            if (strlength < 1) {
                continue;
            }
            //            printf("str_list item:%s",str_list[loop]);
            switch (lf_trie_search_real_data(tst->trie_state, str_list[loop], strlength)) {
                case TRIE_SEARCH_SUCCESS:
                    search_num++;
                    break;
                case TRIE_SEARCH_ERROR:
                    error_num++;
                    break;
                default:
                    break;
            }
        }
        readline++;
        free_strlist(str_list);
        gettimeofday(&deal_end, NULL);
        total_time += 1000000 * (deal_end.tv_sec - deal_start.tv_sec) + deal_end.tv_usec - deal_start.tv_usec;
        //        if(deal_num % 100000 == 0)
        //        {
        //            printf("%u\t%u\t%lf\n",tst->thread_num,deal_num,total_time/1000000);
        //        }
    }
    total_time /= 1000000;
    //test
    //    atom_t tmp = tst->trie_state->trieNodeList->nodelist_counter;
    //    printf("\nnodelist_num:%d\n",tmp);
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    timeused /= 1000000;
    printf("\ntotal_search:%u\t%u\t%lf\t%lf\t%u\t%u\n\n", tst->thread_num, deal_num, timeused, total_time, search_num,
           duplication);
    fflush(stdout);
    return ((thread_return_t) EXIT_SUCCESS);
}

void memtrie_test_internal_insert_and_search(void) {
    unsigned int loop, cpu_count;
    thread_state_t *thread_handles;
    struct trie_state *ts;
    struct trie_state_test *tst;
    atom_t data, thread, count, *per_thread_counters;
    struct validation_info vi = {1000000, 1000000};
    enum data_structure_validity dvs[2];
    internal_display_test_name("MemTrie insert and search...");
    cpu_count = abstraction_cpu_count();

    if (!(trie_new(&ts, 16))) {
        internal_display_test_name("MemTrie new over...");
        return;
    }
    internal_display_test_name("MemTrie test  2...");
    tst = abstraction_malloc(sizeof(struct trie_state_test) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++) {
        (tst + loop)->trie_state = ts;
        //        (tst + loop)->init_node_counter = (atom_t)loop << (sizeof(atom_t) * 8 - 8);
    }
    thread_handles = abstraction_malloc(sizeof(thread_state_t) * cpu_count);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_start(&thread_handles[loop], loop, memtrie_test_internal_thread_insert_and_search,
                                 tst + loop);
    for (loop = 0; loop < cpu_count; loop++)
        abstraction_thread_wait(thread_handles[loop]);
    abstraction_free(thread_handles);
    abstraction_free(tst);
    //test
    trie_state_free(ts);
    internal_display_test_name("trie_insert_and_search over !");
    return;
}

thread_return_t CALLING_CONVENTION memtrie_test_internal_thread_insert_and_search(void *state) {
    struct trie_state_test *tst;
    struct hash_data *data;
    atom_t insert_num = 0, error_num = 0, duplication = 0, readline = 0, maxline = 0;
    int strlength = 0, deal_num = 0;
    char **current_str, **str_list;

    assert(NULL != state);
    tst = (struct trie_state_test *) state;
    trie_use(tst->trie_state);
    FILE *read_file = fopen(FILEPATH, "r");
    if (NULL == read_file) {
        printf("open file error");
        return (thread_return_t) EXIT_FAILURE;
    }
    readline = (tst->read_line) * (tst->thread_num);
    maxline = (tst->read_line) * (tst->thread_num + 1);
    read2startline(readline, read_file);
    //    char* test = "abcdefghijklmn",*str;
    //    strlen = 14;
    struct timeval tpstart, tpend, deal_start, deal_end;
    double total_time = 0.0;
    char *str = (char *) malloc(sizeof(char) * STRLEN_MAX);
    gettimeofday(&tpstart, NULL);
    while (readline < maxline + 1 && fgets_str(str, read_file)) {
        //printf("start split");
        str_list = split(str, ' ');
        gettimeofday(&deal_start, NULL);
        int loop;
        for (loop = 0; loop < 3; loop++) {
            deal_num++;
            //            printf("\ndeal_num:%d\n",deal_num);
            strlength = strlen(str_list[loop]);
            if (strlength < 1) {
                continue;
            }
            //            printf("str_list item:%s",str_list[loop]);
            switch (lf_trie_insert_real_data(tst->trie_state, str_list[loop], strlength)) {
                case TRIE_INSERT_SUCCESS:
                    insert_num++;
                    break;
                case TRIE_INSERT_ERROR:
                    error_num++;
                    break;
                case TRIE_INSERT_DUPLICATION:
                    duplication++;
                default:
                    break;
            }
        }
        free_strlist(str_list);
        gettimeofday(&deal_end, NULL);
        total_time += 1000000 * (deal_end.tv_sec - deal_start.tv_sec) + deal_end.tv_usec - deal_start.tv_usec;
        if (deal_num % 100000 == 0) {
            printf("pthread_t:%d\tdeal_num:%u\ttime:%lf\n", pthread_self(), deal_num, total_time / 1000000);
        }
    }
    total_time /= 1000000;
    //test
    //    atom_t tmp = tst->trie_state->trieNodeList->nodelist_counter;
    //    printf("\nnodelist_num:%d\n",tmp);
    gettimeofday(&tpend, NULL);
    double timeused = 1000000 * (tpend.tv_sec - tpstart.tv_sec) + tpend.tv_usec - tpstart.tv_usec;
    timeused /= 1000000;
    printf("\npthread_t:%u", pthread_self());
    printf("\nall execute time : %lf seconds\treal insert execute time:%lf seconds\n", timeused, total_time);
    printf("insert_num:%u   error_num:%u   duplication:%u\n\n", insert_num, error_num, duplication);
    return ((thread_return_t) EXIT_SUCCESS);
}

void memtrie_test_internal_rapid_put_and_get(void) {

}

