
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define STATIC_THRESHOLD 0.05   // 静止加速度变化阈值
#define ACCELERATION_THRESHOLD 0.1 // 加速度变化最小阈值
#define JERK_THRESHOLD 0.2     // 抖动方差阈值
#define JERK_WINDOW 20          // 抖动判断窗口大小，增加窗口大小

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

// 归一化加速度数据
double normalize(double value, double min, double max) {
    return (max - min) == 0 ? 0 : (value - min) / (max - min);
}

// 计算窗口内数据的均值
double calculate_mean(DataPoint *window, int size) {
    double sum = 0.0;
    for (int i = 0; i < size; i++) {
        sum += window[i].ag;
    }
    return sum / size;
}

// 计算窗口内数据的方差
double calculate_variance(DataPoint *window, int size) {
    if (size < 2) return 0.0;
    
    double min_ag = window[0].ag, max_ag = window[0].ag;
    for (int i = 1; i < size; i++) {
        if (window[i].ag < min_ag) min_ag = window[i].ag;
        if (window[i].ag > max_ag) max_ag = window[i].ag;
    }
    
    double sum = 0.0, mean, variance = 0.0;
    for (int i = 0; i < size; i++) {
        sum += normalize(window[i].ag, min_ag, max_ag);
    }
    mean = sum / size;
    
    for (int i = 0; i < size; i++) {
        variance += pow(normalize(window[i].ag, min_ag, max_ag) - mean, 2);
    }
    return variance / size;
}

// 使用方差和均值变化检测抖动
int is_jerky(DataPoint *window, int size) {
    if (size < JERK_WINDOW) return 0;
    double variance = calculate_variance(window, size);
    double mean_diff = calculate_mean(window + size / 2, size / 2) - calculate_mean(window, size / 2);
    printf("time:%f, ag:%f, variance:%f, mean_diff:%f\n", window->time, window->ag, variance, mean_diff);
    return variance > JERK_THRESHOLD && fabs(mean_diff) < ACCELERATION_THRESHOLD;
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
    } else if (delta_ag > ACCELERATION_THRESHOLD) {
        new_state = ACCELERATING;
    } else if (delta_ag < -ACCELERATION_THRESHOLD) {
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

int main(int argc, char** argv) {
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
