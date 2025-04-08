
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define STATIC_THRESHOLD 0.05   // 静止加速度变化阈值
#define JERK_THRESHOLD 0.5      // 抖动加速度变化阈值
#define JERK_WINDOW 5           // 抖动判断窗口大小

typedef struct {
    double time;
    double ag;
} DataPoint;

// 判断是否为抖动：在短时间内加速度方向频繁切换
int is_jerky(DataPoint *data, int index, int size) {
    if (index < JERK_WINDOW) return 0; // 需要足够的数据点
    
    int direction_changes = 0;
    for (int i = index - JERK_WINDOW; i < index - 1; i++) {
        if ((data[i + 1].ag - data[i].ag) * (data[i].ag - data[i - 1].ag) < 0) {
            direction_changes++;
        }
    }
    return direction_changes > (JERK_WINDOW / 2); // 若变化超过窗口一半，则判定为抖动
}

void analyze_motion(DataPoint *data, int size) {
    const char *current_state = "static";
    double start_time = data[0].time;
    
    for (int i = 1; i < size; i++) {
        double delta_ag = data[i].ag - data[i - 1].ag;
        const char *new_state;
        
        if (fabs(data[i].ag) < STATIC_THRESHOLD) {
            new_state = "static";
        } else if (is_jerky(data, i, size)) {
            new_state = "jerk";
        } else if (delta_ag > STATIC_THRESHOLD) {
            new_state = "accelerating";
        } else if (delta_ag < -STATIC_THRESHOLD) {
            new_state = "decelerating";
        } else {
            new_state = "constant speed";
        }
        
        if (strcmp(current_state, new_state) != 0) {
            printf("%s: %.3f - %.3f\n", current_state, start_time, data[i - 1].time);
            current_state = new_state;
            start_time = data[i].time;
        }
    }
    printf("%s: %.3f - %.3f\n", current_state, start_time, data[size - 1].time);
}

int main(int argc, char** argv) {
    FILE *file = fopen(argv[1]/*"acceleration_data.csv"*/, "r");
    if (!file) {
        perror("文件打开失败");
        return 1;
    }
    
    DataPoint data[10000]; // 假设最多 10000 行数据
    int count = 0;
    char line[256];
    
    fgets(line, sizeof(line), file); // 跳过表头
    while (fgets(line, sizeof(line), file) && count < 10000) {
        sscanf(line, "%lf,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%*f,%lf", &data[count].time, &data[count].ag);
        count++;
    }
    fclose(file);
    
    analyze_motion(data, count);
    
    return 0;
}
