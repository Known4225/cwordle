/*
cwordle - wordle cracker

Written by Ryan Srichai, 19.09.2026
*/

#include "turtle.h"
#include <time.h>

typedef enum {
    CWORLDLE_MODE_GRAPH = 0,
    CWORDLE_MODE_CHAIN = 1,
} cwordle_mode_t;

enum {
    GRAPH_COLOR_BLACK,
    GRAPH_COLOR_YELLOW,
    GRAPH_COLOR_GREEN,
};

enum {
    GRAPH_TOP_LETTER, // char
    GRAPH_TOP_COLOR, // GRAPH_COLOR_X
    GRAPH_TOP_FREQUENCY, // int
    GRAPH_TOP_NUMBER_OF_FIELDS,
};

enum {
    GRAPH_POINT_LETTER, // char
    GRAPH_POINT_COLOR, // GRAPH_COLOR_X
    GRAPH_POINT_NUMBER_OF_FIELDS,
};

enum {
    CWORDLE_COLOR_BLACK,
    CWORDLE_COLOR_YELLOW,
    CWORDLE_COLOR_GREEN,
    CWORDLE_COLOR_TEXT,
    CWORDLE_COLOR_BAR,
    CWORDLE_COLOR_BAR_HIGHLIGHT,
    CWORDLE_COLOR_GRAPH,
};

enum {
    FINAL_WORD,
    FINAL_SCORE,
    FINAL_VARIANCE,
    FINAL_FREQUENCY,
    FINAL_NUMBER_OF_FIELDS,
};

enum {
    KEY_LMB,
    KEY_RMB,
    KEY_SPACE,
};

typedef struct {
    list_t *points; // raw data points collected from analysing past words (unused as of now)
    list_t *top; // data points sorted by frequency
    int32_t numberOfBars; // number of bars on the graph (and data points in top)
    int8_t strict;
    double leftX;
    double rightX;
    double topY;
    double bottomY;
} cwordle_graph_t;

typedef struct {
    cwordle_mode_t mode;
    list_t *pastWordsRaw; // straight loaded from file
    list_t *validWords; // list of valid words
    list_t *validWordsEligible; // list of ints, one for each validWord which says how many times that word is a guessable word after the first guess
    int8_t removeDuplicates; // pastWords include duplicates unless this is set to 1
    list_t *pastWords; // list of past words in order
    list_t *duplicateWords; // list of duplicate words
    list_t *swordleBest; // list of best words from swordle (and variance of each)
    list_t *finalRankings; // words ranked by their combined score (using (1 / variance from swordle) * (frequency of applicability))
    int8_t overrideFinal;
    int32_t totalPairs;
    uint32_t lookup[26]; // array of bitfields for each character (for use in get_possible_words function)
    int8_t lookupMatrix[36]; // starting matrix for wagner-fischer algorithm (for use in get_edit_distance)
    int32_t search; // number of top words to search for blobs of (a blob is like a chain but with a hub and spokes topology)
    char keys[8];
    cwordle_graph_t graph;
} cwordle_t;

cwordle_t self;

int32_t monthToInt(const char *month);
void wordle_simulate(int32_t *greenBucket, int32_t *yellowBucket, int32_t *blackBucket, const char *guess, const char *answer);
void wordle_simulate_points(list_t *points, int32_t *greenBucket, int32_t *yellowBucket, int32_t *blackBucket, const char *guess, const char *answer);
void wordle_simulate_check_words(int32_t *greenBucket, int32_t *yellowBucket, int32_t *blackBucket, const char *guess, const char *answer);
int32_t get_possible_words(list_t *output, char *canvas, list_t *wordSet);
double get_chain_coverage(list_t *chain);
int8_t get_edit_distance(const char *word1, const char *word2);
list_t *get_adjacent_words(char *word, list_t *wordSet);
void cwordle_set_color(int32_t color);

int8_t cwordle_colors[] = {
    58, 58, 60, // CWORDLE_COLOR_BLACK
    181, 159, 59, // CWORDLE_COLOR_YELLOW
    83, 141, 78, // CWORDLE_COLOR_GREEN
    248, 248, 248, // CWORDLE_COLOR_TEXT
    180, 180, 180, // CWORDLE_COLOR_BAR
    0, 180, 180, // CWORDLE_COLOR_BAR_HIGHLIGHT
    255, 255, 255, // CWORDLE_COLOR_GRAPH
};

int32_t init() {
    self.mode = CWORLDLE_MODE_GRAPH;
    /* load files */
    char constructedFilepath[5120];
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "wordle-past-words.csv");
    self.pastWordsRaw = os_tools_load_csv_string(constructedFilepath, OSTOOLS_CSV_ROW);
    if (self.pastWordsRaw == NULL) {
        printf("Could not load %s\n", constructedFilepath);
        return -1;
    }
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "wordle-valid-words.csv");
    self.validWords = list_init();
    FILE *validfp = fopen(constructedFilepath, "r");
    if (validfp == NULL) {
        printf("Could not open %s\n", constructedFilepath);
        return -1;
    }
    char validWord[10];
    while (fgets(validWord, 10, validfp) != NULL) {
        for (int32_t i = 0; i < 5; i++) {
            if (validWord[i] >= 97 && validWord[i] <= 122) {
                validWord[i] -= 32;
            }
        }
        validWord[5] = '\0';
        list_append(self.validWords, (unitype) validWord, 's');
    }
    fclose(validfp);
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "swordle-best.list");
    FILE *swordlefp = fopen(constructedFilepath, "r");
    if (swordlefp == NULL) {
        printf("Could not open %s\n", constructedFilepath);
        return -1;
    }
    self.swordleBest = list_read(swordlefp);
    fclose(swordlefp);
    self.validWordsEligible = list_init();
    for (int32_t i = 0; i < self.validWords -> length; i++) {
        list_append(self.validWordsEligible, (unitype) 0, 'i');
    }
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "config.txt");
    list_t *configFile = os_tools_load_internal(constructedFilepath, OSTOOLS_CSV_ROW, ':', OSTOOLS_CSV_FIELD_STRING);
    if (configFile == NULL) {
        printf("Could not load %s\n", constructedFilepath);
        return -1;
    }
    for (int32_t i = 0; i < configFile -> length; i++) {
        if (streq(configFile -> data[i].r -> data[0].s, "removeDuplicates")) {
            sscanf(configFile -> data[i].r -> data[1].s, "%hhd", &self.removeDuplicates);
        }
        if (streq(configFile -> data[i].r -> data[0].s, "numberOfBars")) {
            sscanf(configFile -> data[i].r -> data[1].s, "%d", &self.graph.numberOfBars);
        }
        if (streq(configFile -> data[i].r -> data[0].s, "strictData")) {
            sscanf(configFile -> data[i].r -> data[1].s, "%hhd", &self.graph.strict);
        }
        if (streq(configFile -> data[i].r -> data[0].s, "overrideFinal")) {
            sscanf(configFile -> data[i].r -> data[1].s, "%hhd", &self.overrideFinal);
        }
        if (streq(configFile -> data[i].r -> data[0].s, "search")) {
            sscanf(configFile -> data[i].r -> data[1].s, "%d", &self.search);
        }
    }
    /* get_possible_words */
    for (int32_t i = 0; i < 26; i++) {
        self.lookup[i] = 1 << i;
    }
    /* get_edit_distance */
    for (int32_t i = 0; i < 6; i++) {
        self.lookupMatrix[i] = i;
    }
    for (int32_t i = 1; i < 6; i++) {
        self.lookupMatrix[i * 6] = i;
    }
    /* generate pastWords */
    self.pastWords = list_init();
    self.duplicateWords = list_init();
    for (int32_t i = self.pastWordsRaw -> length - 1; i > 0; i--) {
        int32_t firstIndex = list_find(self.pastWords, self.pastWordsRaw -> data[i].r -> data[4], 's');
        if (firstIndex != -1) {
            firstIndex = self.pastWordsRaw -> length - firstIndex - 1;
            // printf("index %d, firstIndex %d\n", self.pastWordsRaw -> length - i, self.pastWordsRaw -> length - firstIndex);
            // printf("index %d, firstIndex %d\n", i, firstIndex);
            char fullCode[1024];
            sprintf(fullCode, "%s    %s.%02d.%s and %s.%02d.%s", self.pastWordsRaw -> data[i].r -> data[4].s,
                                                                 self.pastWordsRaw -> data[firstIndex].r -> data[1].s, monthToInt(self.pastWordsRaw -> data[firstIndex].r -> data[0].s), self.pastWordsRaw -> data[firstIndex].r -> data[2].s,
                                                                 self.pastWordsRaw -> data[i].r -> data[1].s, monthToInt(self.pastWordsRaw -> data[i].r -> data[0].s), self.pastWordsRaw -> data[i].r -> data[2].s);
            list_append(self.duplicateWords, (unitype) fullCode, 's');
            if (self.removeDuplicates) {
                continue;
            }
        }
        list_append(self.pastWords, self.pastWordsRaw -> data[i].r -> data[4], 's');
    }

    /* calculate graph data - the question to answer is... how many times would i get this data point if i were to use each of these words as my starting word */
    self.graph.points = list_init(); // unused
    int32_t greenBucket[26] = {0};
    int32_t yellowBucket[26] = {0};
    int32_t blackBucket[26] = {0};
    self.totalPairs = 0;
    if (self.graph.strict) {
        /* in strict mode, we obtain 5 * (self.pastWords -> length - 1) data points by applying the question from each word to the next in the sequence (collect 5 data points from applying the word on 01.01.2025 to 02.01.2025) */
        for (int32_t i = 0; i < self.pastWords -> length - 1; i++) {
            if (self.overrideFinal) {
                wordle_simulate(greenBucket, yellowBucket, blackBucket, self.pastWords -> data[i].s, self.pastWords -> data[i + 1].s);
            } else {
                wordle_simulate_check_words(greenBucket, yellowBucket, blackBucket, self.pastWords -> data[i].s, self.pastWords -> data[i + 1].s);
            }
            self.totalPairs++;
            if (i % 10 == 0 && self.overrideFinal == 0) {
                turtle_clear();
                turtle_pen_color(0, 0, 0);
                turtle_rectangle(-200, -10, 200, 10);
                cwordle_set_color(CWORDLE_COLOR_GREEN);
                double length = (198 * 2.0 * i) / (self.pastWords -> length - 1) - 198;
                turtle_rectangle(-198, -8, length, 8);
                turtle_update();
            }
        }
        // wordle_simulate_points(self.graph.points, "CLICK", "CAPON");
        // wordle_simulate_points(self.graph.points, "GEODE", "LOOSE");
    } else {
        /* in non-strict mode, we obtain 5 * (self.pastWords -> length) * (self.pastWords -> length - 1) data points by applying the question to every other word (collect 5 * (self.pastWords -> length - 1) data points from applying the word on 01.01.2025 to every other word) */
        for (int32_t i = 0; i < self.pastWords -> length; i++) {
            for (int32_t j = 0; j < self.pastWords -> length; j++) {
                if (i == j) {
                    continue;
                }
                self.totalPairs++;
                if (self.overrideFinal) {
                    wordle_simulate(greenBucket, yellowBucket, blackBucket, self.pastWords -> data[i].s, self.pastWords -> data[j].s);
                } else {
                    wordle_simulate_check_words(greenBucket, yellowBucket, blackBucket, self.pastWords -> data[i].s, self.pastWords -> data[j].s);
                }
            }
            if (self.overrideFinal == 0) {
                turtle_clear();
                turtle_pen_color(0, 0, 0);
                turtle_rectangle(-200, -10, 200, 10);
                cwordle_set_color(CWORDLE_COLOR_GREEN);
                double length = (198 * 2.0 * i) / self.pastWords -> length - 198;
                turtle_rectangle(-198, -8, length, 8);
                turtle_update();
            }
        }
    }
    self.graph.top = list_init();
    for (int32_t i = 0; i < 26; i++) {
        list_append(self.graph.top, (unitype) ('A' + i), 'c'); // GRAPH_TOP_LETTER
        list_append(self.graph.top, (unitype) GRAPH_COLOR_GREEN, 'i'); // GRAPH_TOP_COLOR
        list_append(self.graph.top, (unitype) greenBucket[i], 'i'); // GRAPH_TOP_FREQUENCY
        list_append(self.graph.top, (unitype) ('A' + i), 'c'); // GRAPH_TOP_LETTER
        list_append(self.graph.top, (unitype) GRAPH_COLOR_YELLOW, 'i'); // GRAPH_TOP_COLOR
        list_append(self.graph.top, (unitype) yellowBucket[i], 'i'); // GRAPH_TOP_FREQUENCY
        list_append(self.graph.top, (unitype) ('A' + i), 'c'); // GRAPH_TOP_LETTER
        list_append(self.graph.top, (unitype) GRAPH_COLOR_BLACK, 'i'); // GRAPH_TOP_COLOR
        list_append(self.graph.top, (unitype) blackBucket[i], 'i'); // GRAPH_TOP_FREQUENCY
    }
    list_sort_stride(self.graph.top, GRAPH_TOP_NUMBER_OF_FIELDS, GRAPH_TOP_FREQUENCY);
    if (self.overrideFinal == 0) {
        /* calculate final rankings */
        self.finalRankings = list_init();
        for (int32_t i = 0; i < self.validWords -> length; i++) {
            int32_t index = list_find(self.swordleBest, self.validWords -> data[i], 's');
            if (index == -1) {
                printf("Could not find %s in swordle-best.list\n", self.validWords -> data[i].s);
                continue;
            }
            double finalScore = (1.0 / self.swordleBest -> data[index + 1].d) * self.validWordsEligible -> data[i].i;
            list_append(self.finalRankings, self.validWords -> data[i], 's');
            list_append(self.finalRankings, (unitype) finalScore, 'd');
            list_append(self.finalRankings, self.swordleBest -> data[index + 1], 'd');
            list_append(self.finalRankings, self.validWordsEligible -> data[i], 'i');
        }
        list_sort_stride(self.finalRankings, 4, 1);
    } else {
        strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
        strcat(constructedFilepath, "cwordle-final-rankings.list");
        FILE *finalfp = fopen(constructedFilepath, "r");
        if (finalfp == NULL) {
            printf("Could not open %s\n", constructedFilepath);
            return -1;
        }
        self.finalRankings = list_read(finalfp);
        fclose(finalfp);
    }

    /* find chains */
    list_t *testChain = list_init();
    // list_append(testChain, (unitype) "TIARE", 's');
    // list_append(testChain, (unitype) "ROATE", 's');
    // list_append(testChain, (unitype) "TARSE", 's');
    // list_append(testChain, (unitype) "SOARE", 's');
    // list_append(testChain, (unitype) "IRATE", 's');
    // list_append(testChain, (unitype) "STARE", 's');
    // list_append(testChain, (unitype) "RAISE", 's');
    // list_append(testChain, (unitype) "SATER", 's');
    // list_append(testChain, (unitype) "RAILE", 's');
    // list_append(testChain, (unitype) "ARISE", 's');
    // list_append(testChain, (unitype) "ORATE", 's');
    // list_append(testChain, (unitype) "STRAE", 's');
    // list_append(testChain, (unitype) "TALER", 's');
    // list_append(testChain, (unitype) "REAST", 's');
    // list_append(testChain, (unitype) "TASER", 's');
    // list_append(testChain, (unitype) "LATER", 's');

    // list_append(testChain, (unitype) "QAJAQ", 's');
    // list_append(testChain, (unitype) "JEEZE", 's');
    // list_append(testChain, (unitype) "XVIII", 's');
    // list_append(testChain, (unitype) "JAFFA", 's');
    // list_append(testChain, (unitype) "MAMMA", 's');
    // list_append(testChain, (unitype) "PZAZZ", 's');
    // list_append(testChain, (unitype) "AGGAG", 's');
    // list_append(testChain, (unitype) "FEEZE", 's');
    // list_append(testChain, (unitype) "MEZZE", 's');
    // list_append(testChain, (unitype) "MAQAM", 's');
    // list_append(testChain, (unitype) "JAZZY", 's');
    // list_append(testChain, (unitype) "EXEME", 's');
    // list_append(testChain, (unitype) "PEEPE", 's');
    // list_append(testChain, (unitype) "EXEEM", 's');
    // list_append(testChain, (unitype) "IMMIX", 's');
    // list_append(testChain, (unitype) "ADDAX", 's');

    list_append(testChain, (unitype) "ROATE", 's');
    // list_append(testChain, (unitype) "SIDER", 's');
    // list_append(testChain, (unitype) "SEINE", 's');
    // list_append(testChain, (unitype) "SLIMY", 's');
    // list_append(testChain, (unitype) "NALAS", 's');
    // list_append(testChain, (unitype) "DOOLY", 's');
    // list_append(testChain, (unitype) "DOVER", 's');
    // list_append(testChain, (unitype) "SLANK", 's');
    // list_append(testChain, (unitype) "LENES", 's');
    // list_append(testChain, (unitype) "SNOOL", 's');
    // list_append(testChain, (unitype) "SHUNT", 's');
    // list_append(testChain, (unitype) "CURLS", 's');
    // list_append(testChain, (unitype) "CROON", 's');
    // list_append(testChain, (unitype) "SLAKE", 's');
    // list_append(testChain, (unitype) "GENAL", 's');
    // list_append(testChain, (unitype) "TAUNT", 's');
    // list_append(testChain, (unitype) "CRANS", 's');
    // list_append(testChain, (unitype) "MACLE", 's');
    // list_append(testChain, (unitype) "LAGER", 's');
    // list_append(testChain, (unitype) "SLEET", 's');
    // list_append(testChain, (unitype) "PRISE", 's');
    // list_append(testChain, (unitype) "CARRS", 's');
    // list_append(testChain, (unitype) "SALON", 's');
    // list_append(testChain, (unitype) "CLOSE", 's');
    // list_append(testChain, (unitype) "ENTER", 's');
    // list_append(testChain, (unitype) "ARMOR", 's');
    // list_append(testChain, (unitype) "TRIPS", 's');
    // list_append(testChain, (unitype) "SPOOT", 's');
    // list_append(testChain, (unitype) "TOMIN", 's');
    // list_append(testChain, (unitype) "CLEAT", 's');

    // printf("coverage: %lf\n", get_chain_coverage(testChain));

    // printf("%d\n", get_edit_distance("TIARE", "ROATE"));
    // printf("%d\n", get_edit_distance("CLASS", "FAULT"));
    // printf("%d\n", get_edit_distance("ROATE", "ROATE"));
    // printf("%d\n", get_edit_distance("ROATE", "ROSTE"));

    if (self.search != 0) {
        list_t *chains = list_init();
        for (int32_t i = 0; i < self.search; i++) {
            list_t *adj = get_adjacent_words(self.finalRankings -> data[i * FINAL_NUMBER_OF_FIELDS].s, self.validWords);
            list_insert(adj, 0, self.finalRankings -> data[i * FINAL_NUMBER_OF_FIELDS], 's');
            double score = 0;
            for (int32_t j = 0; j < adj -> length; j++) {
                int32_t index = list_find(self.finalRankings, adj -> data[j], 's');
                if (index == -1) {
                    printf("ERROR: Could not find %s in finalRankings\n", adj -> data[j].s);
                    continue;
                }
                score += self.finalRankings -> data[index + FINAL_SCORE].d;
            }
            score /= 1000;
            double coverage = get_chain_coverage(adj);
            score *= coverage;
            list_insert(adj, 0, (unitype) coverage, 'd');
            list_insert(adj, 0, (unitype) score, 'd');
            list_append(chains, (unitype) score, 'd');
            list_append(chains, (unitype) adj, 'r');
            turtle_clear();
            turtle_pen_color(0, 0, 0);
            turtle_rectangle(-200, -10, 200, 10);
            cwordle_set_color(CWORDLE_COLOR_GREEN);
            double length = (198 * 2.0 * i) / self.search - 198;
            turtle_rectangle(-198, -8, length, 8);
            turtle_update();
        }
        list_sort_stride(chains, 2, 0);
        for (int32_t i = chains -> length - 2; i >= 0; i -= 2) {
            list_delete(chains, i);
        }
        list_print(chains);
    }

    list_t *cares = get_adjacent_words("CARES", self.validWords);
    list_insert(cares, 0, (unitype) "CARES", 's');
    list_t *caresSorted = list_init();
    for (int32_t i = 0; i < cares -> length; i++) {
        int32_t index = list_find(self.finalRankings, cares -> data[i], 's');
        if (index == -1) {
            printf("ERROR: Could not find %s in finalRankings\n", cares -> data[i].s);
            continue;
        }
        list_append(caresSorted, (unitype) (1.0 / self.finalRankings -> data[index + FINAL_VARIANCE].d * 1000), 'd');
        list_append(caresSorted, cares -> data[i], 's');
    }
    list_sort_stride(caresSorted, 2, 0);
    for (int32_t i = caresSorted -> length - 2; i >= 0; i -= 2) {
        list_delete(caresSorted, i);
    }
    list_print(caresSorted);

    /* graph */
    self.graph.leftX = -280;
    self.graph.rightX = 280;
    self.graph.topY = 140;
    self.graph.bottomY = -130;

    /* readers */
    // turtle_tools_reader_init("Past Words", (unitype *) &self.pastWords, UNITYPE_LIST, -135, 90, 10);
    // turtle_tools_reader_init("Duplicate Words", (unitype *) &self.duplicateWords, UNITYPE_LIST, 5, 90, 10);
    // turtle_tools_reader_init("Valid Words", (unitype *) &self.validWords, UNITYPE_LIST, -135, 90, 10);
    // turtle_tools_reader_init("Valid Words Eligible", (unitype *) &self.validWordsEligible, UNITYPE_LIST, 5, 90, 10);
    turtle_tools_reader_init("Final Rankings", (unitype *) &self.finalRankings, UNITYPE_LIST, -135, 90, 10);
    return 0;
}

void cwordle_set_color(int32_t color) {
    turtle_pen_color(cwordle_colors[color * 3], cwordle_colors[color * 3 + 1], cwordle_colors[color * 3 + 2]);
}

void render_tile(char letter, int32_t color, double x, double y, double size) {
    cwordle_set_color(color);
    turtle_rectangle(x - size / 2, y - size / 2, x + size / 2, y + size / 2);
    cwordle_set_color(CWORDLE_COLOR_TEXT);
    char str[2];
    str[0] = letter;
    str[1] = '\0';
    turtle_text_write_string(str, x, y, size * 0.7, 50);
}

void render_graph() {
    if (self.mode != CWORLDLE_MODE_GRAPH) {
        return;
    }
    double sizeTile = (self.graph.rightX - self.graph.leftX) / (self.graph.numberOfBars);
    double xTile = self.graph.leftX + sizeTile / 2;
    double maxValue = round(self.graph.top -> data[0 + GRAPH_TOP_FREQUENCY].i * 1.1);
    for (int32_t i = 0; i < self.graph.numberOfBars; i++) {
        int32_t topIndex = i * GRAPH_TOP_NUMBER_OF_FIELDS;
        /* render tile */
        render_tile(self.graph.top -> data[topIndex + GRAPH_TOP_LETTER].c, self.graph.top -> data[topIndex + GRAPH_TOP_COLOR].c, xTile, self.graph.bottomY - sizeTile * 0.55 - 1, sizeTile * 0.85);
        /* render bar */
        cwordle_set_color(CWORDLE_COLOR_BAR);
        turtle_rectangle(xTile - sizeTile * 0.425, self.graph.bottomY, xTile + sizeTile * 0.425, self.graph.top -> data[topIndex + GRAPH_TOP_FREQUENCY].i / maxValue * (self.graph.topY - self.graph.bottomY) + self.graph.bottomY);
        xTile += sizeTile;
    }
    /* render graph */
    cwordle_set_color(CWORDLE_COLOR_GRAPH);
    turtle_pen_size(1);
    turtle_goto(self.graph.rightX, self.graph.bottomY);
    turtle_pen_down();
    turtle_goto(self.graph.leftX - 1, self.graph.bottomY);
    turtle_goto(self.graph.leftX - 1, self.graph.topY);
    turtle_pen_up();
    /* render ticks */
    
}

void mouse() {
    if (turtle_key_pressed(GLFW_KEY_SPACE) && turtle_key_pressed(GLFW_KEY_LEFT_CONTROL)) {
        if (self.keys[KEY_SPACE] == 0) {
            self.keys[KEY_SPACE] = 1;
            FILE *fp = fopen("cwordle-final-rankings-coverage.list", "w");
            list_write(fp, self.finalRankings);
            fclose(fp);
        }
    } else {
        self.keys[KEY_SPACE] = 0;
    }
}

void wordle_simulate(int32_t *greenBucket, int32_t *yellowBucket, int32_t *blackBucket, const char *guess, const char *answer) {
    int8_t cache[26] = {0};
    int8_t colors[5] = {0};
    for (int32_t i = 0; i < 5; i++) {
        if (guess[i] == answer[i]) {
            colors[i] = GRAPH_COLOR_GREEN;
        } else {
            cache[answer[i] - 'A']++;
        }
    }
    for (int32_t i = 0; i < 5; i++) {
        int8_t letter = guess[i];
        if (colors[i] == GRAPH_COLOR_BLACK) {
            if (cache[letter - 'A'] > 0 && (answer[0] == letter || answer[1] == letter || answer[2] == letter || answer[3] == letter || answer[4] == letter)) {
                cache[letter - 'A']--;
                colors[i] = GRAPH_COLOR_YELLOW;
            }
        }
        switch (colors[i]) {
            case GRAPH_COLOR_GREEN:
                greenBucket[letter - 'A']++;
            break;
            case GRAPH_COLOR_YELLOW:
                yellowBucket[letter - 'A']++;
            break;
            case GRAPH_COLOR_BLACK:
                blackBucket[letter - 'A']++;
            break;
        }
    }
}

void wordle_simulate_points(list_t *points, int32_t *greenBucket, int32_t *yellowBucket, int32_t *blackBucket, const char *guess, const char *answer) {
    int32_t startingIndex = points -> length;
    int8_t cache[26] = {0};
    for (int32_t i = 0; i < 5; i++) {
        list_append(points, (unitype) guess[i], 'c');
        if (guess[i] == answer[i]) {
            list_append(points, (unitype) GRAPH_COLOR_GREEN, 'i');
        } else {
            list_append(points, (unitype) GRAPH_COLOR_BLACK, 'i');
            cache[answer[i] - 'A']++;
        }
    }
    for (int32_t i = startingIndex; i < points -> length; i += GRAPH_POINT_NUMBER_OF_FIELDS) {
        int8_t letter = points -> data[i + GRAPH_POINT_LETTER].c;
        if (points -> data[i + GRAPH_POINT_COLOR].i == GRAPH_COLOR_BLACK) {
            if (cache[letter - 'A'] > 0 && (answer[0] == letter || answer[1] == letter || answer[2] == letter || answer[3] == letter || answer[4] == letter)) {
                cache[letter - 'A']--;
                points -> data[i + GRAPH_POINT_COLOR].i = GRAPH_COLOR_YELLOW;
            }
        }
        switch (points -> data[i + GRAPH_POINT_COLOR].i) {
            case GRAPH_COLOR_GREEN:
                greenBucket[letter - 'A']++;
            break;
            case GRAPH_COLOR_YELLOW:
                yellowBucket[letter - 'A']++;
            break;
            case GRAPH_COLOR_BLACK:
                blackBucket[letter - 'A']++;
            break;
        }
    }
    /* print */
    printf("%s and %s: %d %d %d %d %d\n", guess, answer,
                               points -> data[startingIndex + 0 * GRAPH_POINT_NUMBER_OF_FIELDS + GRAPH_POINT_COLOR].i,
                               points -> data[startingIndex + 1 * GRAPH_POINT_NUMBER_OF_FIELDS + GRAPH_POINT_COLOR].i,
                               points -> data[startingIndex + 2 * GRAPH_POINT_NUMBER_OF_FIELDS + GRAPH_POINT_COLOR].i,
                               points -> data[startingIndex + 3 * GRAPH_POINT_NUMBER_OF_FIELDS + GRAPH_POINT_COLOR].i,
                               points -> data[startingIndex + 4 * GRAPH_POINT_NUMBER_OF_FIELDS + GRAPH_POINT_COLOR].i);
}

void wordle_simulate_check_words(int32_t *greenBucket, int32_t *yellowBucket, int32_t *blackBucket, const char *guess, const char *answer) {
    int8_t cache[26] = {0};
    int8_t colors[5] = {0};
    char constructedCanvas[12] = {0};
    for (int32_t i = 0; i < 5; i++) {
        constructedCanvas[i * 2] = guess[i];
        if (guess[i] == answer[i]) {
            colors[i] = GRAPH_COLOR_GREEN;
        } else {
            cache[answer[i] - 'A']++;
        }
    }
    for (int32_t i = 0; i < 5; i++) {
        int8_t letter = guess[i];
        if (colors[i] == GRAPH_COLOR_BLACK) {
            if (cache[letter - 'A'] > 0 && (answer[0] == letter || answer[1] == letter || answer[2] == letter || answer[3] == letter || answer[4] == letter)) {
                cache[letter - 'A']--;
                colors[i] = GRAPH_COLOR_YELLOW;
            }
        }
        switch (colors[i]) {
            case GRAPH_COLOR_GREEN:
                greenBucket[letter - 'A']++;
            break;
            case GRAPH_COLOR_YELLOW:
                yellowBucket[letter - 'A']++;
            break;
            case GRAPH_COLOR_BLACK:
                blackBucket[letter - 'A']++;
            break;
        }
        constructedCanvas[i * 2 + 1] = colors[i];
    }
    get_possible_words(self.validWordsEligible, constructedCanvas, self.validWords);
}

int32_t get_possible_words(list_t *output, char *canvas, list_t *wordSet) {
    if (output -> length != wordSet -> length) {
        return 0;
    }
    /* create word whitelist and global count */
    int8_t count[26] = {0}; // need to have at least count[letter] of a particular letter, if count[letter] is negative then you need to have exactly -count[letter] in a word
    uint32_t whitelist[5] = {0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF}; // so i just learned today that you can only do one of these when it is 0
    int8_t currentCount[26] = {0};
    for (int32_t i = 0; i < 5; i++) {
        switch (canvas[i * 2 + 1]) {
            case CWORDLE_COLOR_GREEN:;
                if (currentCount[canvas[i * 2] - 'A'] < 0) {
                    currentCount[canvas[i * 2] - 'A']--;
                } else {
                    currentCount[canvas[i * 2] - 'A']++;
                }
                whitelist[i] = self.lookup[canvas[i * 2] - 'A']; // canvas uses capital letters
            break;
            case CWORDLE_COLOR_YELLOW:;
                for (int32_t k = 0; k < i; k++) {
                    if (canvas[k * 2] == canvas[i * 2] && canvas[k * 2 + 1] == CWORDLE_COLOR_BLACK) {
                        // printf("getPossibleWords: Invalid canvas configuration %d %d\n", i, j);
                        return 0;
                    }
                }
                currentCount[canvas[i * 2] - 'A']++; // cannot ever have currentCountDirection set as a black letter can never proceed a yellow letter
                whitelist[i] &= ~self.lookup[canvas[i * 2] - 'A']; // canvas uses capital letters
            break;
            case CWORDLE_COLOR_BLACK:;
                uint32_t blacklist = ~self.lookup[canvas[i * 2] - 'A']; // canvas uses capital letters
                int32_t startingIndex = i;
                if (currentCount[canvas[i * 2] - 'A'] == 0) {
                    for (int32_t k = 0; k < 5; k++) {
                        whitelist[k] &= blacklist;
                    }
                } else {
                    currentCount[canvas[i * 2] - 'A'] *= -1;
                    whitelist[i] &= ~self.lookup[canvas[i * 2] - 'A'];
                }
            break;
        }
        for (int32_t i = 0; i < 26; i++) {
            if (count[i] >= 0 && abs(currentCount[i]) >= count[i]) {
                count[i] = currentCount[i];
            }
        }
    }
    /* gather all possible words given canvas into output */
    int32_t outputLen = 0;
    for (int32_t i = 0; i < wordSet -> length; i++) {
        char *word = wordSet -> data[i].s;
        char good = 1;
        int8_t currentCount[26] = {0};
        /* check whitelist */
        for (int32_t j = 0; j < 5; j++) {
            currentCount[word[j] - 'A']++;
            if ((whitelist[j] & self.lookup[word[j] - 'A']) == 0) { // wordlists use capital letters
                good = 0;
                break;
            }
        }
        for (int32_t i = 0; i < 26; i++) {
            /* check if minimum global count is met */
            if (abs(count[i]) > currentCount[i]) {
                good = 0;
                break;
            }
            /* check if exact global count is met (if information is available) */
            if (count[i] < 0 && currentCount[i] != -count[i]) {
                good = 0;
                break;
            }
        }
        if (good) {
            outputLen++;
            output -> data[i].i++;
        }
    }
    return outputLen;
}

int32_t is_covered(const char *guess, const char *answer, list_t *wordSet) {
    /* wordle simulate */
    int8_t cache[26] = {0};
    int8_t colors[5] = {0};
    char canvas[12] = {0};
    for (int32_t i = 0; i < 5; i++) {
        canvas[i * 2] = guess[i];
        if (guess[i] == answer[i]) {
            colors[i] = GRAPH_COLOR_GREEN;
        } else {
            cache[answer[i] - 'A']++;
        }
    }
    for (int32_t i = 0; i < 5; i++) {
        int8_t letter = guess[i];
        if (colors[i] == GRAPH_COLOR_BLACK) {
            if (cache[letter - 'A'] > 0 && (answer[0] == letter || answer[1] == letter || answer[2] == letter || answer[3] == letter || answer[4] == letter)) {
                cache[letter - 'A']--;
                colors[i] = GRAPH_COLOR_YELLOW;
            }
        }
        canvas[i * 2 + 1] = colors[i];
    }

    /* check possible words */
    /* create word whitelist and global count */
    int8_t count[26] = {0}; // need to have at least count[letter] of a particular letter, if count[letter] is negative then you need to have exactly -count[letter] in a word
    uint32_t whitelist[5] = {0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF, 0x3FFFFFF}; // so i just learned today that you can only do one of these when it is 0
    int8_t currentCount[26] = {0};
    for (int32_t i = 0; i < 5; i++) {
        switch (canvas[i * 2 + 1]) {
            case CWORDLE_COLOR_GREEN:;
                if (currentCount[canvas[i * 2] - 'A'] < 0) {
                    currentCount[canvas[i * 2] - 'A']--;
                } else {
                    currentCount[canvas[i * 2] - 'A']++;
                }
                whitelist[i] = self.lookup[canvas[i * 2] - 'A']; // canvas uses capital letters
            break;
            case CWORDLE_COLOR_YELLOW:;
                for (int32_t k = 0; k < i; k++) {
                    if (canvas[k * 2] == canvas[i * 2] && canvas[k * 2 + 1] == CWORDLE_COLOR_BLACK) {
                        // printf("getPossibleWords: Invalid canvas configuration %d %d\n", i, j);
                        return 0;
                    }
                }
                currentCount[canvas[i * 2] - 'A']++; // cannot ever have currentCountDirection set as a black letter can never proceed a yellow letter
                whitelist[i] &= ~self.lookup[canvas[i * 2] - 'A']; // canvas uses capital letters
            break;
            case CWORDLE_COLOR_BLACK:;
                uint32_t blacklist = ~self.lookup[canvas[i * 2] - 'A']; // canvas uses capital letters
                int32_t startingIndex = i;
                if (currentCount[canvas[i * 2] - 'A'] == 0) {
                    for (int32_t k = 0; k < 5; k++) {
                        whitelist[k] &= blacklist;
                    }
                } else {
                    currentCount[canvas[i * 2] - 'A'] *= -1;
                    whitelist[i] &= ~self.lookup[canvas[i * 2] - 'A'];
                }
            break;
        }
    }
    for (int32_t i = 0; i < 26; i++) {
        if (count[i] >= 0 && abs(currentCount[i]) >= count[i]) {
            count[i] = currentCount[i];
        }
    }
    /* gather all possible words given canvas into output */
    int32_t outputLen = 0;
    for (int32_t i = 0; i < wordSet -> length; i++) {
        char *word = wordSet -> data[i].s;
        char good = 1;
        int8_t currentCount[26] = {0};
        /* check whitelist */
        for (int32_t j = 0; j < 5; j++) {
            currentCount[word[j] - 'A']++;
            if ((whitelist[j] & self.lookup[word[j] - 'A']) == 0) { // wordlists use capital letters
                good = 0;
                break;
            }
        }
        for (int32_t i = 0; i < 26; i++) {
            /* check if minimum global count is met */
            if (abs(count[i]) > currentCount[i]) {
                good = 0;
                break;
            }
            /* check if exact global count is met (if information is available) */
            if (count[i] < 0 && currentCount[i] != -count[i]) {
                good = 0;
                break;
            }
        }
        if (good) {
            return 1;
        }
    }
    return 0;
}

/* returns a percentage */
double get_chain_coverage(list_t *chain) {
    int32_t covered = 0;
    if (self.graph.strict) {
        /* in strict mode, we obtain 5 * (self.pastWords -> length - 1) data points by applying the question from each word to the next in the sequence (collect 5 data points from applying the word on 01.01.2025 to 02.01.2025) */
        for (int32_t i = 0; i < self.pastWords -> length - 1; i++) {
            covered += is_covered(self.pastWords -> data[i].s, self.pastWords -> data[i + 1].s, chain);
        }
    } else {
        /* in non-strict mode, we obtain 5 * (self.pastWords -> length) * (self.pastWords -> length - 1) data points by applying the question to every other word (collect 5 * (self.pastWords -> length - 1) data points from applying the word on 01.01.2025 to every other word) */
        for (int32_t i = 0; i < self.pastWords -> length; i++) {
            for (int32_t j = 0; j < self.pastWords -> length; j++) {
                if (i == j) {
                    continue;
                }
                covered += is_covered(self.pastWords -> data[i].s, self.pastWords -> data[j].s, chain);
            }
        }
    }
    return (double) covered / self.totalPairs * 100;
}

int8_t min_three(int8_t a, int8_t b, int8_t c) {
    if (a < b) {
        if (a < c) {
            return a;
        }
        return c;
    } else {
        if (b < c) {
            return b;
        }
        return c;
    }
}

/* compute Levenshtein edit distance between two 5-letter words */
int8_t get_edit_distance(const char *word1, const char *word2) {
    int8_t matrix[36];
    memcpy(matrix, self.lookupMatrix, 36);
    for (int32_t i = 1; i < 6; i++) {
        char letter = word1[i - 1];
        for (int32_t j = 1; j < 6; j++) {
            int32_t index = i * 6 + j;
            if (letter == word2[j - 1]) {
                matrix[index] = matrix[index - 7];
            } else {
                matrix[index] = min_three(matrix[index - 1], matrix[index - 6], matrix[index - 7]) + 1;
            }
        }
    }
    return matrix[35];
}

/* get all words that are edit distance 1 from this word */
list_t *get_adjacent_words(char *word, list_t *wordSet) {
    list_t *output = list_init();
    for (int32_t i = 0; i < wordSet -> length; i++) {
        if (get_edit_distance(word, wordSet -> data[i].s) == 1) {
            list_append(output, wordSet -> data[i], 's');
        }
    }
    return output;
}

int32_t monthToInt(const char *month) {
    if (streq(month, "January")) {
        return 1;
    }
    if (streq(month, "February")) {
        return 2;
    }
    if (streq(month, "March")) {
        return 3;
    }
    if (streq(month, "April")) {
        return 4;
    }
    if (streq(month, "May")) {
        return 5;
    }
    if (streq(month, "June")) {
        return 6;
    }
    if (streq(month, "July")) {
        return 7;
    }
    if (streq(month, "August")) {
        return 8;
    }
    if (streq(month, "September")) {
        return 9;
    }
    if (streq(month, "October")) {
        return 10;
    }
    if (streq(month, "November")) {
        return 11;
    }
    if (streq(month, "December")) {
        return 12;
    }
    return -1;
}

void parse_ribbon_output() {
    if (turtleToolsRibbon.output[0] == 0) {
        return;
    }
    turtleToolsRibbon.output[0] = 0;
    if (turtleToolsRibbon.output[1] == 0) { // File
        if (turtleToolsRibbon.output[2] == 1) { // New
            list_clear(osToolsFileDialog.selectedFilenames);
            printf("New\n");
        }
        if (turtleToolsRibbon.output[2] == 2) { // Save
            if (osToolsFileDialog.selectedFilenames -> length == 0) {
                if (os_tools_file_dialog_save(OSTOOLS_FILE_DIALOG_FILE, "Save.txt", NULL) != -1) {
                    printf("Saved to: %s\n", osToolsFileDialog.selectedFilenames -> data[0].s);
                }
            } else {
                printf("Saved to: %s\n", osToolsFileDialog.selectedFilenames -> data[0].s);
            }
        }
        if (turtleToolsRibbon.output[2] == 3) { // Save As...
            list_clear(osToolsFileDialog.selectedFilenames);
            if (os_tools_file_dialog_save(OSTOOLS_FILE_DIALOG_FILE, "Save.txt", NULL) != -1) {
                printf("Saved to: %s\n", osToolsFileDialog.selectedFilenames -> data[0].s);
            }
        }
        if (turtleToolsRibbon.output[2] == 4) { // Open
            list_clear(osToolsFileDialog.selectedFilenames);
            if (os_tools_file_dialog_open(OSTOOLS_FILE_DIALOG_MULTIPLE_SELECT, OSTOOLS_FILE_DIALOG_FILE, "", NULL) != -1) {
                printf("Loaded data from: ");
                list_print(osToolsFileDialog.selectedFilenames);
            }
        }
    }
    if (turtleToolsRibbon.output[1] == 1) { // Edit
        if (turtleToolsRibbon.output[2] == 1) { // Undo
            printf("Undo\n");
        }
        if (turtleToolsRibbon.output[2] == 2) { // Redo
            printf("Redo\n");
        }
        if (turtleToolsRibbon.output[2] == 3) { // Cut
            os_tools_clipboard_set_text("test123");
            printf("Cut \"test123\" to clipboard!\n");
        }
        if (turtleToolsRibbon.output[2] == 4) { // Copy
            os_tools_clipboard_set_text("test345");
            printf("Copied \"test345\" to clipboard!\n");
        }
        if (turtleToolsRibbon.output[2] == 5) { // Paste
            os_tools_clipboard_get_text();
            printf("Pasted \"%s\" from clipboard!\n", osToolsClipboard.text);
        }
    }
    if (turtleToolsRibbon.output[1] == 2) { // View
        if (turtleToolsRibbon.output[2] == 1) { // Change theme
            printf("Change theme\n");
            if (turtleToolsTheme == TT_THEME_DARK) {
                turtle_background_color(36, 30, 32);
                turtle_tools_set_theme(TT_THEME_COLT);
            } else if (turtleToolsTheme == TT_THEME_COLT) {
                turtle_background_color(212, 201, 190);
                turtle_tools_set_theme(TT_THEME_NAVY);
            } else if (turtleToolsTheme == TT_THEME_NAVY) {
                turtle_background_color(255, 255, 255);
                turtle_tools_set_theme(TT_THEME_LIGHT);
            } else if (turtleToolsTheme == TT_THEME_LIGHT) {
                turtle_background_color(30, 30, 30);
                turtle_tools_set_theme(TT_THEME_DARK);
            }
        }
        if (turtleToolsRibbon.output[2] == 2) { // GLFW
            printf("GLFW settings\n");
        }
    }
}

int main(int argc, char *argv[]) {
    /* create window */
    GLFWwindow *window = turtle_create_window_icon(TURTLE_WINDOW_DEFAULT_WIDTH, TURTLE_WINDOW_DEFAULT_HEIGHT, "turtle demo", "images/thumbnail.png");
    if (window == NULL) {
        return -1; // failed to create window
    }

    /* initialise turtle */
    turtle_set_resize_mode(TURTLE_RESIZE_MODE_PAD); // change to TURTLE_RESIZE_MODE_STRETCH to have content stretch when resized
    turtle_init(window, -320, -180, 320, 180);

    /* initialise os_tools */
    os_tools_init(argv[0], window); // must include argv[0] to get executableFilepath, must include GLFW window for copy paste and cursor functionality
    os_tools_file_dialog_add_global_extension("txt"); // add txt to extension restrictions
    os_tools_file_dialog_add_global_extension("csv"); // add csv to extension restrictions

    /* initialise turtle_text */
    char constructedFilepath[5120];
    strcpy(constructedFilepath, osToolsFileDialog.executableFilepath);
    strcat(constructedFilepath, "config/roberto.tgl");
    turtle_text_init(constructedFilepath);

    /* initialise turtle_tools ribbon */
    turtle_tools_set_theme(TT_THEME_DARK); // dark theme preset
    list_t *ribbonConfig = list_init();
    list_append(ribbonConfig, (unitype) "File, 📄 New, 📄 Save, 📄 Save As..., 📄 Open", 's');
    list_append(ribbonConfig, (unitype) "Edit, Undo, Redo, Cut, Copy, Paste", 's');
    list_append(ribbonConfig, (unitype) "View, Change Theme, GLFW", 's');
    turtle_tools_ribbon_init_list(ribbonConfig);

    if (init()) {
        printf("cwordle.exe ERROR: Failed to initialise\n");
        return -1;
    }

    uint32_t tps = 120; // ticks per second (locked to fps in this case)
    clock_t start, end;

    while (turtle.close == 0) {
        start = clock();
        turtle_get_mouse_coordinates();
        turtle_clear();
        render_graph();
        mouse();
        turtle_tools_update(); // update turtleTools
        turtle_tools_set_color(TT_COLOR_TEXT);
        turtle_text_write_stringf(-310, -170, 5, 0, "%.2lf, %.2lf", turtle.mouseX, turtle.mouseY);
        parse_ribbon_output(); // user defined function to use ribbon
        turtle_update(); // update the screen
        end = clock();
        while ((double) (end - start) / CLOCKS_PER_SEC < (1.0 / tps)) {
            end = clock();
        }
    }
    turtle_free();
    return 0;
}
