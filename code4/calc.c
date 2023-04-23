#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct task {
  double a;
  double b;
  double epsilon;
  double delta_x;
  double delta_y;
  double coeff[4];
  double res;
  int finish;
};

struct task *data;

double f(double bad_x, double coeff[], double delta_x, double delta_y) {
  double x = bad_x + delta_x;
  return (coeff[3] * (x * x * x) + coeff[2] * (x * x) + coeff[1] * x +
          coeff[0] + delta_y);
}

double calc(double a, double b, double epsilon, double delta_x, double delta_y,
            double coeff[]) {
  double ans = 0;
  double I1 = ((b - a) / 2) *
              (f(a, coeff, delta_x, delta_y) + f(b, coeff, delta_x, delta_y));
  double m = (a + b) / 2;
  double I2 = ((b - a) / 4) * (f(a, coeff, delta_x, delta_y) +
                               2 * f(m, coeff, delta_x, delta_y) +
                               f(b, coeff, delta_x, delta_y));
  if (fabs(I1 - I2) <= 3 * (b - a) * epsilon) {
    return I2;
  }
  ans += calc(a, m, epsilon, delta_x, delta_y, coeff);
  ans += calc(m, b, epsilon, delta_x, delta_y, coeff);
  return ans;
}

char *sem_cnt[] = {
    "semapone", "semaptwo",   "semapthree", "semapfour", "semapfive",
    "semapsix", "semapseven", "semapeight", "semapnine", "semapten",
};

int shm, n;
char shname[] = "shmem";

void my_handler_parent(int sign) {
  for (int i = 0; i < n; i++) {
    sem_unlink(sem_cnt[i]);
  }
  close(shm);
  if (shm_unlink(shname) == -1) {
    printf("Shared memory is absent\n");
    perror("shm_unlink");
  }
  printf("Semaphores and Shared memory was deleted\n");
  printf("Parent the end\n");
  exit(0);
}

void my_handler_child(int sign) {
  printf("Child the end\n");
  exit(0);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Wrong number of input arguments: %d instead of 1\n", argc - 1);
    return 0;
  }
  n = atoi(argv[1]);
  if (n <= 0 || n > 10) {
    printf("Wrong number of accountants: it must be > 0 and < 11\n");
    return 0;
  }
  double coeff[4];
  double x1, x2, y1, y2, epsilon;
  printf("Write f(x) coefficients: ");
  scanf("%lf%lf%lf%lf", &coeff[0], &coeff[1], &coeff[2], &coeff[3]);
  printf("Write area borders x1, x2, y1, y2: ");
  scanf("%lf%lf%lf%lf", &x1, &x2, &y1, &y2);
  if (x2 <= x1 || y2 <= y1) {
    printf("Incorrect borders\n");
    return 0;
  }
  if (fabs(x1) > 10 || fabs(x2) > 10 || fabs(y1) > 10 || fabs(y2) > 10) {
    printf("Incorrect borders\n");
    return 0;
  }
  printf("Write epsilon: ");
  scanf("%lf", &epsilon);
  double ans = (x2 - x1) * (y2 - y1);
  double delta_x = x1;
  double delta_y = -y1;
  double A = 0;
  double B = x2 - x1;
  sem_t *semaphors[10];
  for (int i = 0; i < n; i++) {
    semaphors[i] = sem_open(sem_cnt[i], O_CREAT, 0666, 0);
  }

  if ((shm = shm_open(shname, O_CREAT | O_RDWR, 0666)) == -1) {
    printf("Object is already open\n");
    perror("shm_open");
    return 1;
  } else {
    printf("Object is open: name = %s, id = 0x%x\n", shname, shm);
  }
  size_t sz = 10 * sizeof(struct task);
  if (ftruncate(shm, sz) == -1) {
    printf("Error with ftruncate\n");
    perror("ftruncate");
    return 1;
  } else {
    printf("Memory size set and = %ld\n", sz);
  }

  data = mmap(0, sz, PROT_WRITE | PROT_READ, MAP_SHARED, shm, 0);

  if (data == MAP_FAILED) {
    printf("Error gwith mmap\n");
    perror("mmap");
    return 1;
  }
  int cnt = 0;
  int res = 0;
  for (int i = 0; i < n; i++) {
    data[cnt].finish = 0;
    res = fork();
    if (res == 0) {
      break;
    }
    cnt++;
  }
  if (res != 0) {
    (void)signal(SIGINT, my_handler_parent);
  }
  if (res == 0) { //счетоводы
    (void)signal(SIGINT, my_handler_child);
    printf("Accountant %d is ready\n", cnt);
    while (1) {
      sem_wait(semaphors[cnt]);
      if (data[cnt].finish == 1) {
        printf("Accountant %d exit\n", cnt);
        exit(0);
      }
      data[cnt].res =
          calc(data[cnt].a, data[cnt].b, data[cnt].epsilon, data[cnt].delta_x,
               data[cnt].delta_y, data[cnt].coeff);
      printf("Accountant %d finished calculation, ans = %lf\n", cnt,
             data[cnt].res);
      data[cnt].finish = 2;
    }
  }
  sleep(1); //ждем пока все счетоводы не будут готовы получать задачи
  double len = (B - A) / 20;
  printf("%lf\n", len);
  double curA = A;
  for (int i = 0; i < 20; i++) {
    if (i < n) {
      data[i].a = curA;
      data[i].b = curA + len;
      curA += len;
      data[i].epsilon = epsilon;
      data[i].delta_x = delta_x;
      data[i].delta_y = delta_y;
      for (int y = 0; y < 4; y++) {
        data[i].coeff[y] = coeff[y];
      }
      sem_post(semaphors[i]);
      continue;
    }
    int cur = i % n;
    while (data[cur].finish == 0) {
      continue;
    }
    ans -= data[cur].res;
    data[cur].a = curA;
    data[cur].b = curA + len;
    curA += len;
    data[cur].finish = 0;
    sem_post(semaphors[cur]);
  }
  //теперь все семафоры ждут конца
  for (int i = 0; i < n; i++) {
    while (data[i].finish == 0) {
      continue;
    }
    ans -= data[i].res;
    data[i].finish = 1;
    sem_post(semaphors[i]);
  }
  sleep(1);
  for (int i = 0; i < n; i++) {
    sem_unlink(sem_cnt[i]);
  }
  close(shm);
  if (shm_unlink(shname) == -1) {
    printf("Shared memory is absent\n");
    perror("shm_unlink");
  }
  printf("Semaphores and Shared memory was deleted\n");
  printf("Area = %lf\n", ans);
}