
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define STATIC_THRESHOLD 0.05   // 静止加速度变化阈值
#define JERK_THRESHOLD 0.02     // 抖动方差阈值
#define JERK_WINDOW 5           // 抖动判断窗口大小

typedef enum {
    STATIC,
    ACCELERATING,
    CONSTANT_SPEED,
    DECELERATING,
    JERK
} MotionState;

typedef struct {
    double time;
    double ag;
} DataPoint;

// 计算窗口内数据的方差
double calculate_variance(DataPoint *window, int size) {
    if (size < 2) return 0.0;
    
    double sum = 0.0, mean, variance = 0.0;
    for (int i = 0; i < size; i++) {
        sum += window[i].ag;
    }
    mean = sum / size;
    
    for (int i = 0; i < size; i++) {
        variance += pow(window[i].ag - mean, 2);
    }
    return variance / size;
}

// 使用方差检测抖动
int is_jerky(DataPoint *window, int size) {
    if (size < JERK_WINDOW) return 0;
    double variance = calculate_variance(window, size);
    return variance > JERK_THRESHOLD;
}

const char* state_to_string(MotionState state) {
    switch (state) {
        case STATIC: return "static";
        case ACCELERATING: return "accelerating";
        case CONSTANT_SPEED: return "constant speed";
        case DECELERATING: return "decelerating";
        case JERK: return "jerk";
        default: return "unknown";
    }
}

void update_state(MotionState *current_state, double *start_time, DataPoint *window, int size) {
    if (size < 2) return;
    
    double delta_ag = window[size - 1].ag - window[size - 2].ag;
    MotionState new_state;
    
    if (is_jerky(window, size)) {
        new_state = JERK;
    } else if (fabs(window[size - 1].ag) < STATIC_THRESHOLD) {
        new_state = STATIC;
    } else if (delta_ag > STATIC_THRESHOLD) {
        new_state = ACCELERATING;
    } else if (delta_ag < -STATIC_THRESHOLD) {
        new_state = DECELERATING;
    } else {
        new_state = CONSTANT_SPEED;
    }
    
    if (*current_state != new_state) {
        printf("%s: %.3f - %.3f\n", state_to_string(*current_state), *start_time, window[size - 2].time);
        *current_state = new_state;
        *start_time = window[size - 1].time;
    }
}

int main(int argc, char ** argv) {
    FILE *file = fopen(argv[1]/*"acceleration_data.csv"*/, "r");
    if (!file) {
        perror("文件打开失败");
        return 1;
    }
    
    DataPoint window[JERK_WINDOW];
    int count = 0;
    char line[256];
    MotionState current_state = STATIC;
    double start_time = 0.0;
    
    fgets(line, sizeof(line), file); // 跳过表头
    while (fgets(line, sizeof(line), file)) {
        DataPoint new_data;
        sscanf(line, "%lf,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%lf", &new_data.time, &new_data.ag);
        
        if (count < JERK_WINDOW) {
            window[count++] = new_data;
            if (count == 1) start_time = new_data.time;
        } else {
            memmove(window, window + 1, (JERK_WINDOW - 1) * sizeof(DataPoint));
            window[JERK_WINDOW - 1] = new_data;
            update_state(&current_state, &start_time, window, JERK_WINDOW);
        }
    }
    printf("%s: %.3f - %.3f\n", state_to_string(current_state), start_time, window[count - 1].time);
    
    fclose(file);
    return 0;
}

