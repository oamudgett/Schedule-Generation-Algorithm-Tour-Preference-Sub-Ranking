#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOTAL_DAYS 76
#define NUM_SCHEDULES 270
#define MIN_WORK_DAYS 12
#define MAX_WORK_DAYS 20
#define MAX_COMBINED_TOURS 30

const int month_days[] = {30, 31, 31, 31, 28};
const char *month_names[] = {"11", "12", "01", "02"};

void generate_dates(char dates[TOTAL_DAYS][6]) {
    int day = 1, month_idx = 0;
    for (int i = 0; i < TOTAL_DAYS; i++) {
        sprintf(dates[i], "%s/%02d", month_names[month_idx], day);
        day++;
        if (day > month_days[month_idx]) {
            day = 1;
            month_idx++;
        }
    }
}

typedef struct {
    int work;
    int rest;
} Tour;

Tour default_sets[][5] = {
    { {4, 3}, {5, 3}, {6, 4} },
    { {4, 3}, {5, 3}, {6, 4}, {7, 4}, {8, 5} }
};
const int default_set_sizes[] = {3, 5};

Tour preference_sets[][10] = {
    {},
    { {4, 4} },
    { {4, 3}, {4, 4}, {4, 5}, {4, 6} },
    { {5, 5}, {6, 5}, {7, 5} },
    { {5, 5}, {6, 6}, {7, 7} },
    { {8, 3}, {9, 4}, {10, 4}, {11, 5}, {12, 5} },
    { {4, 3}, {5, 3}, {6, 4}, {7, 4}, {8, 5}, {4, 4}, {5, 4}, {6, 5}, {7, 5}, {8, 6} },
    { {14, 14} }
};
const int preference_set_sizes[] = {0, 1, 4, 3, 3, 5, 10, 1};

typedef struct {
    int work;
    int rest;
    int rank;
} RankedTour;

RankedTour combined_tours[MAX_COMBINED_TOURS];
int combined_tour_size = 0;

void set_active_tour_set(int default_set, int preference_set) {
    int count = 0;
    for (int i = 0; i < default_set_sizes[default_set]; i++) {
        combined_tours[count++] = (RankedTour){default_sets[default_set][i].work, default_sets[default_set][i].rest, 0};
    }
    for (int i = 0; i < preference_set_sizes[preference_set]; i++) {
        combined_tours[count++] = (RankedTour){preference_sets[preference_set][i].work, preference_sets[preference_set][i].rest, i + 1};
    }
    combined_tour_size = count;
}

int pick_weighted_tour_index() {
    int total_weight = 0;
    int weights[MAX_COMBINED_TOURS];

    for (int i = 0; i < combined_tour_size; i++) {
        weights[i] = combined_tours[i].rank == 0 ? 10 : (int)(10.0 / combined_tours[i].rank);
        total_weight += weights[i];
    }

    int r = rand() % total_weight;
    for (int i = 0, sum = 0; i < combined_tour_size; i++) {
        sum += weights[i];
        if (r < sum) return i;
    }
    return combined_tour_size - 1;
}

void generate_schedule(char schedule[TOTAL_DAYS], char *existing_28day) {
    memset(schedule, ' ', TOTAL_DAYS);

    // Map 28-day bidding period (days 29–52) into new schedule's days 0–23
    memcpy(schedule, existing_28day, 24);

    // Define remaining two periods
    struct { int start, end; } periods[2] = {
        {24, 51}, // New Period 2
        {52, 75}  // Period 3
    };

    for (int p = 0; p < 2; p++) {
        int days_scheduled = 0, retries = 0;
        int work_target = MIN_WORK_DAYS + rand() % (MAX_WORK_DAYS - MIN_WORK_DAYS + 1);

        while (days_scheduled < work_target && retries < 1000) {
            int idx = pick_weighted_tour_index();
            int tour_len = combined_tours[idx].work;
            int rest_len = combined_tours[idx].rest;

            int range = periods[p].end - periods[p].start + 1;
            int start = periods[p].start + rand() % range;

            if (start + tour_len - 1 > periods[p].end) {
                retries++;
                continue;
            }

            int conflict = 0;
            for (int i = 0; i < tour_len; i++) {
                if (schedule[start + i] != ' ') {
                    conflict = 1;
                    break;
                }
            }
            if (conflict) {
                retries++;
                continue;
            }

            for (int i = 0; i < tour_len && days_scheduled < work_target; i++) {
                schedule[start + i] = 'S';
                days_scheduled++;
            }
        }
    }
}

void count_period_workdays(char *schedule, int *p1, int *p2, int *p3) {
    *p1 = *p2 = *p3 = 0;
    for (int i = 0; i < TOTAL_DAYS; i++) {
        if (schedule[i] == 'S') {
            if (i < 24) (*p1)++;
            else if (i >= 28 && i < 52) (*p2)++;
            else if (i >= 56 && i < 76) (*p3)++;
        }
    }
}

int main() {
    srand(time(NULL));
    char schedules[NUM_SCHEDULES][TOTAL_DAYS + 1];
    char dates[TOTAL_DAYS][6];
    generate_dates(dates);
    set_active_tour_set(0, 0);

    FILE *input = fopen("GeneratedSchedule.csv", "r");
    if (!input) {
        printf("Previous schedule file not found.\n");
        return 1;
    }

    char line[8192];
    fgets(line, sizeof(line), input); // Skip header

    for (int s = 0; s < NUM_SCHEDULES; s++) {
        if (!fgets(line, sizeof(line), input)) break;

        char *token = strtok(line, ","); // "Schedule X"
        char prev_segment[25] = {0};

        for (int i = 0; i < TOTAL_DAYS; i++) {
            token = strtok(NULL, ",");
            if (i >= 28 && i < 52) {
                prev_segment[i - 28] = token[0];
            }
        }

        generate_schedule(schedules[s], prev_segment);
    }

    fclose(input);

    FILE *output = fopen("GeneratedSchedule.csv", "w");
    if (!output) {
        printf("Error opening output file.\n");
        return 1;
    }

    fprintf(output, "Schedule ID");
    for (int i = 0; i < TOTAL_DAYS; i++) {
        fprintf(output, ",%s", dates[i]);
    }
    fprintf(output, ",Period 1 Workdays,Period 2 Workdays,Period 3 Workdays\n");

    for (int s = 0; s < NUM_SCHEDULES; s++) {
        int p1 = 0, p2 = 0, p3 = 0;
        count_period_workdays(schedules[s], &p1, &p2, &p3);

        fprintf(output, "Schedule %d", s + 1);
        for (int i = 0; i < TOTAL_DAYS; i++) {
            fprintf(output, ",%c", schedules[s][i]);
        }
        fprintf(output, ",%d,%d,%d\n", p1, p2, p3);
    }

    fclose(output);
    printf("Schedules generated successfully using prior 28-day period data.\n");
    return 0;
}
