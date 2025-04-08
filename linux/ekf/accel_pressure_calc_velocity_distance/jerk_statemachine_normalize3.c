#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define THRESHOLD 0.08          // 静止加速度阈值
#define ACCELERATION_THRESHOLD 0.1       // 加速度变化阈值
#define JERK_VARIANCE_THRESHOLD 0.02     // 抖动方差阈值
#define JERK_WINDOW_SIZE 10           // 抖动判断窗口大小
#define STATE_DURATION_THRESHOLD 3     // 状态持续时间阈值
#define STATIC_THRESHOLD 0.05         // 静止状态加速度阈值，添加 STATIC_THRESHOLD 定义

// 状态枚举 (修正: 添加 STATE_CONSTANT_SPEED)
typedef enum {
    STATE_STATIC,
    STATE_ACCELERATING,
    STATE_CONSTANT_SPEED, // <-- 添加了 STATE_CONSTANT_SPEED
    STATE_DECELERATING,
    STATE_JERK
} MotionState;

typedef struct {
    double time;
    double ag;
} DataPoint;


// 计算窗口内加速度数据的方差 (用于抖动检测)
double calculate_jerk_variance(DataPoint *window, int size) {
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

// 检测抖动条件 (仅使用方差判断)
int detect_jerk_condition(DataPoint *window, int size) {
    if (size < JERK_WINDOW_SIZE) return 0;
    double variance = calculate_jerk_variance(window, size);
    return variance > JERK_VARIANCE_THRESHOLD;
}

const char* motion_state_to_string(MotionState state) {
    switch (state) {
        case STATE_STATIC: return "静止";
        case STATE_ACCELERATING: return "加速";
        case STATE_CONSTANT_SPEED: return "匀速";
        case STATE_DECELERATING: return "减速";
        case STATE_JERK: return "抖动";
        default: return "未知状态";
    }
}

void update_motion_state(MotionState *current_state, double *start_time, DataPoint *window, int size, int *state_duration_counter, MotionState *last_stable_state) {
    if (size < 2) return;

    double delta_ag = window[size - 1].ag - window[size - 2].ag;
    MotionState new_state;

    if (detect_jerk_condition(window, size)) {
        new_state = STATE_JERK;
    } else if (fabs(window[size - 1].ag) < STATIC_THRESHOLD) { // 使用 STATIC_THRESHOLD 而不是 THRESHOLD
        new_state = STATE_STATIC;
    } else if (delta_ag > ACCELERATION_THRESHOLD) {
        new_state = STATE_ACCELERATING;
    } else if (delta_ag < -ACCELERATION_THRESHOLD) {
        new_state = STATE_DECELERATING;
    } else {
        new_state = STATE_CONSTANT_SPEED; // 正确的状态枚举值
    }

    if (*current_state != new_state) {
        if (*state_duration_counter >= STATE_DURATION_THRESHOLD || new_state == STATE_JERK || new_state == STATE_STATIC) { // 静止也立即切换
            printf("%s 状态结束: %.3f - %.3f\n", motion_state_to_string(*current_state), *start_time, window[size - 2].time);
            if (new_state != STATE_JERK && new_state != STATE_STATIC) {
                *last_stable_state = new_state;
            }
            *current_state = new_state;
            *start_time = window[size - 1].time;
            *state_duration_counter = 0;
        } else {
            (*state_duration_counter)++;
        }
    } else {
        *state_duration_counter = 0;
    }
}


int main() {
    FILE *file = fopen("add-lineaccelaration.csv", "r"); // 使用您的文件名
    if (!file) {
        perror("文件打开失败");
        return 1;
    }

    DataPoint window[JERK_WINDOW_SIZE];
    int count = 0;
    char line[256];
    MotionState current_state = STATE_STATIC; // 初始状态为静止，符合常识
    MotionState last_stable_state = STATE_STATIC;
    double start_time = 0.0;
    int state_duration_counter = 0;
    double ag_buffer[WINDOW_SIZE] = {0.0};
    int buffer_index = 0;
    double smoothed_ag = 0.0;
    double prev_smoothed_ag = 0.0;
    double time, dt, accel_x, accel_y, accel_z, union_g, gyro_x, gyro_y, gyro_z, pressure, temp, ag;

    fgets(line, sizeof(line), file); // 跳过表头
    bool first_line = true;

    int acceleration_intervals = 0;
    int deceleration_intervals = 0;
    int constant_speed_intervals = 0;
    int jerk_intervals = 0;
    MotionState last_interval_state = STATE_STATIC; // 初始状态假设为静止


    while (fgets(line, sizeof(line), file)) {
        DataPoint new_data;
        if (sscanf(line, "%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf",
                   &time, &dt, &accel_x, &accel_y, &accel_z, &union_g, &gyro_x, &gyro_y, &gyro_z, &pressure, &temp, &ag) != 12) {
            fprintf(stderr, "数据解析错误，跳过行: %s", line);
            continue;
        }
        new_data.time = time;
        new_data.ag = ag;


        // 移动平均滤波器实现 (与之前相同)
        ag_buffer[buffer_index % WINDOW_SIZE] = ag;
        buffer_index++;
        double sum = 0.0;
        for (int i = 0; i < WINDOW_SIZE; i++) {
            sum += ag_buffer[i];
        }
        smoothed_ag = sum / WINDOW_SIZE;
        new_data.ag = smoothed_ag;


        if (count < JERK_WINDOW_SIZE) {
            window[count++] = new_data;
            if (count == 1) start_time = new_data.time;
        } else {
            memmove(window, window + 1, (JERK_WINDOW_SIZE - 1) * sizeof(DataPoint));
            window[JERK_WINDOW_SIZE - 1] = new_data;
            update_motion_state(¤t_state, &start_time, window, JERK_WINDOW_SIZE, &state_duration_counter, &last_stable_state);
        }
        prev_smoothed_ag = smoothed_ag;

        MotionState interval_state;
        if (detect_jerk_condition(window, JERK_WINDOW_SIZE)) {
            interval_state = STATE_JERK;
        } else if (fabs(smoothed_ag) < STATIC_THRESHOLD) {
            interval_state = STATE_STATIC;
        } else if (smoothed_ag - prev_smoothed_ag > ACCELERATION_THRESHOLD) {
            interval_state = STATE_ACCELERATING;
        } else if (smoothed_ag - prev_smoothed_ag < -ACCELERATION_THRESHOLD) {
            interval_state = STATE_DECELERATING;
        } else {
            interval_state = STATE_CONSTANT_SPEED;
        }

        if (last_interval_state != interval_state) {
            if (interval_state == STATE_ACCELERATING) acceleration_intervals++;
            else if (interval_state == STATE_DECELERATING) deceleration_intervals++;
            else if (interval_state == STATE_CONSTANT_SPEED) constant_speed_intervals++;
            else if (interval_state == STATE_JERK) jerk_intervals++;
            last_interval_state = interval_state;
        }
        prev_smoothed_ag = smoothed_ag;
    }
     // 确保最后一个区间也被计数
    if (last_interval_state == STATE_ACCELERATING) acceleration_intervals++;
    else if (last_interval_state == STATE_DECELERATING) deceleration_intervals++;
    else if (last_interval_state == STATE_CONSTANT_SPEED) constant_speed_intervals++;
    else if (last_interval_state == STATE_JERK) jerk_intervals++;


    printf("\n区间统计:\n");
    printf("加速区间数量: %d\n", acceleration_intervals);
    printf("减速区间数量: %d\n", deceleration_intervals);
    printf("匀速区间数量: %d\n", constant_speed_intervals);
    printf("抖动区间数量: %d\n", jerk_intervals);


    fclose(file);
    return 0;
}