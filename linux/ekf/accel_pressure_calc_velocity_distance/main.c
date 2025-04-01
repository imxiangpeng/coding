
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define EKF_N 3  // 状态变量 [h, v, a]
#define EKF_M 1  // 观测变量 [h_m (气压计算), a_m (加速度)]
                 //
#define _float_t double

#include "tinyekf.h"

struct dm_ekf {
  ekf_t ekf;
};

static /*const*/ double Q[EKF_N * EKF_N] = {
    1e-1, 0,    0,
    0,    1e-1, 0,
    0,    0,    1e-2
};

static const double R[EKF_M * EKF_M] = {
    // 1e-3, 0,
    // 0,    1e-3
    1E-2
};

static double pressure_to_altitude(double pressure) {
    return (1.0 - pow(pressure / 1013.25, 0.1903)) * 44330.0;
}


static void dm_ekf_init(struct dm_ekf *self) {
    // 初始化 EKF，使用单位协方差矩阵
    const double pdiag[EKF_N] = {1, 1, 1};
    ekf_initialize(&self->ekf, pdiag);
}

static double distance = 0;
static double speed = 0;
static void dm_ekf_run_model (struct dm_ekf *self, double dt, double measured_accel, double measured_pressure) {
    ekf_t *ekf = &self->ekf;
    
    double measured_height = pressure_to_altitude(measured_pressure);

    // 状态转移矩阵 F_k
    /*const*/ double F[EKF_N * EKF_N] = {
        1, dt, 0.5 * dt * dt,
        0, 1,  dt,
        0, 0,  1
    };

    // 观测矩阵 H_k
    const double H[EKF_M * EKF_N] = {
        //0, 0, 0,
        0, 0, 1
    };

    // 预测状态
    /*const*/ double fx[EKF_N] = {
        ekf->x[0] + ekf->x[1] * dt + 0.5 * ekf->x[2] * dt * dt,
        ekf->x[1] + ekf->x[2] * dt,
        ekf->x[2]
    };

    if (fabs(ekf->x[1]) < 0.1 && fabs(measured_accel) < 0.09) {
      printf("ZUPT ...............\n");
        fx[1] = 0;
        ekf->x[1] = 0;  // 速度置 0
        // ekf->P[EKF_N + 1] = 1e-6;  // 速度误差极小，避免恢复
        //Q[ EKF_N + 1] = 1e-6;  // 降低速度噪声
        F[1] = 0;
        F[EKF_N + 1] = 0;
    }

    ekf_predict(ekf, fx, F, Q);

#if 0
    // 低通滤波
    float alpha = 0.1;
    static double accel_filtered = 0;
    accel_filtered = alpha * measured_accel + (1 - alpha) * accel_filtered;
  
    measured_accel = accel_filtered;
#endif

    if (fabs(measured_accel) < 0.1) measured_accel = 0;
 
    distance += speed * dt + 0.5 * measured_accel * dt * dt;
    speed += measured_accel * dt;
    if (measured_accel == 0) {
        if (fabs(speed) < 0.3)
            speed = 0;
    }
     
    printf("speed:%f, distance:%f\n", speed, distance);
    printf("h:%f, a:%f\n", measured_pressure, measured_accel);
    // 观测值
    const double z[EKF_M] = {/*measured_height,*/ measured_accel};

    // 预测测量值
    const double hx[EKF_M] = {/*ekf->x[0],*/ ekf->x[2]};

    printf("z:(%f,%f) vs h:(%f,%f)\n", z[0], z[1], hx[0], hx[1]);
    ekf_update(ekf, z, hx, H, R);
}

#define MAX_LINE_LENGTH 1000

// time,dt,accel_x,accel_y,accel_z,union_g,gyro_x,gyro_y,gyro_z,pressure,temp,ag
// 0.088057,0.068285,-0.21546,-0.234612,-9.820188,-9.825353,0,-0.005325,0.00426,97488.03,31.83,-0.000731482
void process_csv(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("无法打开 CSV 文件");
        return;
    }

    struct dm_ekf self;
    dm_ekf_init(&self);

    char line[MAX_LINE_LENGTH];
    fgets(line, MAX_LINE_LENGTH, file); // 跳过 CSV 头部

    double default_hight = 0;
    int i = 0;
    while (fgets(line, MAX_LINE_LENGTH, file)) {

        double now, dt, accel, pressure, ag;

#if 0        
        if (sscanf(line, "%lf,%lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%*lf,%lf,%*lf,%lf", &now, &dt, &accel, &pressure, &ag) != 5) {
            printf("CSV 解析错误:%s\n", line);
            continue;
        }
#else
        if (sscanf(line, "%lf,%*lf,%*lf,%*lf,%lf,%*lf,%*lf,%lf", &now, &accel, &ag) != 3) {
            printf("CSV 解析错误:%s\n", line);
            continue;
        }
        if (i++ < 400) continue;

        static double prev_time = 0;
        dt = now - prev_time;
        prev_time = now;
#endif

        printf("csv now:%.3f dt: %.3f s, accel: %.3f(%.03f), pressure: %.3f Pa\n", 
            now, dt, accel, ag, pressure);
        if (default_hight == 0) {
          default_hight = pressure_to_altitude(pressure/100);
          //self.ekf.x[0] = default_hight;
        }
        printf("pressure:%f -> high:%f ----> %f\n", pressure / 100, pressure_to_altitude(pressure/100),  pressure_to_altitude(pressure/100) - default_hight);

        dm_ekf_run_model(&self, dt, ag/*accel*/, pressure/100);

        printf("dt: %.3f s, Height: %.3f m, Velocity: %.3f m/s, Acceleration: %.3f m/s²\n",
               dt,
               self.ekf.x[0],
               self.ekf.x[1],
               self.ekf.x[2]);
        char cmd[128] = {0};
        // snprintf(cmd, sizeof(cmd), "echo %lf >> result.csv", self.ekf.x[2]);
        snprintf(cmd, sizeof(cmd), "echo %lf,%lf,%lf >> result.csv", self.ekf.x[2],self.ekf.x[1], self.ekf.x[0]);
        system(cmd);
    }

    fclose(file);
}
int main(int argc, char** argv) {

  if (argc < 2) {
    return -1;
  }
  process_csv(argv[1]);

  return 0;
}
