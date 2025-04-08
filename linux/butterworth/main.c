#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "butterworth_filter.h"

// 海平面标准气压 (Pa)
#define P0 101325.0

// 温度递减率 (K/m)
#define L 0.0065

static const double PRESSURE_L = 0.0065;
static const double PRESSURE_R = 8.31432;
static const double PRESSURE_M = 0.0289644;

static const double G0 = 9.829267;



#define MAX_LINE_LENGTH 1000

double calculate_altitude(double pressure, double temperature) {
    static double fac = PRESSURE_L * PRESSURE_R / PRESSURE_M / G0;
    return ((temperature + 273.15) / L) * (1 - pow(pressure / P0, fac /*0.190284*/));
}

// 计算两次测量之间的高度变化
double calculate_height_difference(double pressure1, double pressure2, double temperature) {
    static double fac = PRESSURE_L * PRESSURE_R / PRESSURE_M / G0;
    return ((temperature + 273.15) / L) * (1 - pow(pressure2 / pressure1, fac /*0.190284*/));
}



// time,dt,accel_x,accel_y,accel_z,union_g,gyro_x,gyro_y,gyro_z,pressure,temp,ag
// 0.088057,0.068285,-0.21546,-0.234612,-9.820188,-9.825353,0,-0.005325,0.00426,97488.03,31.83,-0.000731482
void process_csv(ButterworthFilter *filter, const char *filename) {
    char buf[512] = {0};
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("无法打开 CSV 文件");
        return;
    }

    FILE *result_fd = fopen("result.csv", "w+");
    if (!result_fd) {
        printf("can not open result.csv\n");
        perror("open error:");
        fclose(file);
        return;
    }

    // system("echo 'accel,accel_variance,velocity,velocity_variance,distance,pressure0,pressure1,temp0,temp1,high,high*300,highraw' > result.csv");

    //snprintf(buf, sizeof(buf), "accel,velocity,distance,pressure,temp,altitude,high,delat_accel\n");
    snprintf(buf, sizeof(buf), "accel,filter_accel,velocity,distance,pressure,temp,altitude,high\n");
    
    fwrite(buf, 1, strlen(buf), result_fd);

    char line[MAX_LINE_LENGTH];
    fgets(line, MAX_LINE_LENGTH, file); // 跳过 CSV 头部

    double default_hight = 0;
    int i = 0;
    
    double velocity = 0;
    double distance = 0;
    while (fgets(line, MAX_LINE_LENGTH, file)) {

        double now, dt, accel, pressure, temp, ag;

#if 1        
        if (sscanf(line, "%lf,%lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%*lf,%lf,%lf,%lf", &now, &dt, &accel, &pressure, &temp, &ag) != 6) {
            printf("CSV 解析错误:%s\n", line);
            continue;
        }
        //if (i++ < 440) continue;
#else
        if (sscanf(line, "%lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%lf", &now, &accel, &ag) != 3) {
            printf("CSV 解析错误:%s\n", line);
            continue;
        }
        //if (i++ < 400) continue;

        static double prev_time = 0;
        dt = now - prev_time;
        prev_time = now;
#endif

        printf("csv now:%.3f dt: %.3f s, accel: %.3f(%.03f), pressure: %.3f Pa, temp:%f\n", 
            now, dt, accel, ag, pressure, temp);
        if (default_hight == 0) {
          default_hight = calculate_altitude(pressure, temp);
          //self.ekf.x[0] = default_hight;
        }
        printf("pressure:%f -> high:%f ----> %f\n", pressure / 100, calculate_altitude(pressure, temp),  calculate_altitude(pressure, temp) - default_hight);

        ///dm_ekf_run_model(&self, dt, ag/*accel*/, pressure, temp);
        double fa = butterworth_filter_process(filter, ag);

        //printf("dt: %.3f s, Height: %.3f m, Velocity: %.3f m/s, Acceleration: %.3f m/s², Delat Accel:%.3f\n",
        //       dt,
        //       self.ekf.x[0],
        //       self.ekf.x[1],
        //       self.ekf.x[2],
        //       self.ekf.x[3]);
        if (fabs(fa) > 0.03) {
            velocity += dt * fa;
            distance += velocity *dt + 0.5 * fa * dt * dt;

            printf("speed:%f, distance:%f\n", velocity, distance);
        }
        if (fabs(velocity) < 0.1 && fabs(fa) < 0.03) {
            printf("ZUTP\n");
            velocity = 0;
        }
            printf("speed:%f, distance:%f\n", velocity, distance);
        double altitude = calculate_altitude(pressure, temp);
        //snprintf(buf, sizeof(buf), "%f,%f,%f,%f,%f,%f,%f,%f\n", self.ekf.x[2], self.ekf.x[1], self.ekf.x[0], pressure, temp, altitude, altitude - default_hight, self.ekf.x[3]);
        snprintf(buf, sizeof(buf), "%f,%f,%f,%f,%f,%f,%f,%f\n", ag, fa, velocity, distance, pressure,temp, altitude, altitude - default_hight);
        fwrite(buf, 1, strlen(buf), result_fd);
    }
    

    fclose(result_fd);
    fclose(file);
}
int main(int argc, char** argv) {
    double cutoff_freq = 6; // 截止频率 (Hz)，根据抖动信号频率调整
    double sample_rate = 100.0; // 采样率 (Hz)，根据你的加速度计数据采样率设置
  if (argc < 2) {
    return -1;
  }
     ButterworthFilter* filter = create_butterworth_filter(cutoff_freq, sample_rate);
    if (filter == NULL) {
        return 1; // 错误退出
    } 
  process_csv(filter, argv[1]);

  destroy_butterworth_filter(filter);
  return 0;
}