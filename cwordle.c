/*
cwordle - wordle cracker

Written by Ryan Srichai, 19.09.2026
*/

#include "turtle.h"
#include <time.h>

typedef enum {
    CWORLDLE_MODE_GRAPH = 0,
} cwordle_mode_t;

enum {
    GRAPH_COLOR_BLACK,
    GRAPH_COLOR_YELLOW,
    GRAPH_COLOR_GREEN,
};

enum {
    GRAPH_DATA_LETTER, // char
    GRAPH_DATA_COLOR, // GRAPH_COLOR_X
    GRAPH_DATA_FREQUENCY, // int
    GRAPH_DATA_NUMBER_OF_FIELDS,
};

enum {
    GRAPH_POINT_LETTER, // char
    GRAPH_DATA_COLOR, // GRAPH_COLOR_X
    GRAPH_DATA_NUMBER_OF_FIELDS,
};

typedef struct {
    list_t *points;
    list_t *data; // data points collected from analysing past words
    list_t *top; // top X data points of data (by frequency)
    int32_t numberOfBars; // number of bars on the graph (and data points in top)
    int8_t strict;
} cwordle_graph_t;

typedef struct {
    cwordle_mode_t mode;
    list_t *pastWordsRaw; // straight loaded from file
    list_t *validWords; // list of valid words
    int8_t removeDuplicates; // pastWords include duplicates unless this is set to 1
    list_t *pastWords; // list of past words in order
    list_t *duplicateWords; // list of duplicate words
    cwordle_graph_t graph;
} cwordle_t;

cwordle_t self;

int32_t monthToInt(const char *month);

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
    strcat(constructedFilepath, "wordle-past-words.csv");
    self.validWords = os_tools_load_csv_string(constructedFilepath, OSTOOLS_CSV_ROW);
    if (self.validWords == NULL) {
        printf("Could not load %s\n", constructedFilepath);
        return -1;
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
            sscanf(configFile -> data[i].r -> data[1].s, "%d", &self.removeDuplicates);
        }
        if (streq(configFile -> data[i].r -> data[0].s, "numberOfBars")) {
            sscanf(configFile -> data[i].r -> data[1].s, "%d", &self.graph.numberOfBars);
        }
        if (streq(configFile -> data[i].r -> data[0].s, "strictData")) {
            sscanf(configFile -> data[i].r -> data[1].s, "%d", &self.graph.strict);
        }
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
    // turtle_tools_reader_init("Past Words", (unitype *) &self.pastWords, UNITYPE_LIST, -135, 90, 10);
    // turtle_tools_reader_init("Duplicate Words", (unitype *) &self.duplicateWords, UNITYPE_LIST, 5, 90, 10);

    /* calculate graph data - the question to answer is... how many times would i get this data point if i were to use each of these words as my starting word */
    self.graph.data = list_init();
    self.graph.points = list_init();
    if (self.graph.strict) {
        /* in strict mode, we obtain 5 * (self.pastWords -> length - 1) data points by applying the question from each word to the next in the sequence (collect 5 data points from applying the word on 01.01.2025 to 02.01.2025) */
        for (int32_t index = 0; index < self.pastWords -> length - 1; index++) {
            wordleSimulate(self.graph.points, self.pastWords -> data[index].s, self.pastWords -> data[index + 1].s);
        }
    } else {
        /* in non-strict mode, we obtain 5 * (self.pastWords -> length) * (self.pastWords -> length - 1) data points by applying the question to every other word (collect 5 * (self.pastWords -> length - 1) data points from applying the word on 01.01.2025 to every other word) */
    }
    // for (int32_t point = 0; point < self)

    /* calculate graph top */
    self.graph.top = list_init();

    return 0;
}

void renderGraph() {
    if (self.mode != CWORLDLE_MODE_GRAPH) {
        return;
    }
}

void wordleSimulate(list_t *points, const char *word1, const char *word2) {
    int8_t cache[5] = {0};
    for (int32_t i = 0; i < 5; i++) { 
        list_append(points, (unitype) word1[i], 'c');
        if (word1[i] == word2[i]) {
            list_append(points, (unitype) GRAPH_COLOR_GREEN, 'i');
            cache[i]++;
        } else {
            list_append(points, (unitype) GRAPH_COLOR_BLACK, 'i');
        }
    }
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
