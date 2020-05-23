#include <stdlib.h>
#include <math.h>

// #define REAL double

int n = 5;
int currentX = 0;

// static double
// leastSquare2(int n, const REAL xList[], const REAL yList[])
// {
//     double a;
//     double xsum = 0, x2sum = 0, ysum = 0, xysum = 0;

//     for (int i = 0;i < n; ++i)
//     {
//         xsum = xsum + xList[i];                        //calculate sigma(xi)
//         ysum = ysum + yList[i];                        //calculate sigma(yi)
//         x2sum = x2sum + (xList[i]*xList[i]);                //calculate sigma(x^2i)
//         xysum = xysum + xList[i] * yList[i];                    //calculate sigma(xi*yi)
//     }
//     a = (n * xysum - xsum * ysum) / (n * x2sum - xsum * xsum);            //calculate slope
//     return a;
// }

double ysum = 0;
double xysum = 0;
static double
addValue(double y)
{
    // xsum += currentX;                        //calculate sigma(xi)
    ysum += y;                        //calculate sigma(yi)
    // x2sum += (currentX * currentX);                //calculate sigma(x^2i)
    xysum += currentX * y;                    //calculate sigma(xi*yi)

    ++currentX;
}

double xsum = 0;
double x2sum = 0;
static void
computeXSum()
{
    for(int i = 0; i < n; ++i) {
        xsum += i;
        x2sum += (i*i);
    }
}

static double
computeLeastSquare()
{
    a = (n * xysum - xsum * ysum) / (n * x2sum - xsum * xsum);
}


// void leastSquare(int n, const REAL xList[], const REAL yList[], REAL* m, REAL* b){
//     REAL   x = 0.0;
//     REAL   x2 = 0.0;
//     REAL   xy = 0.0;
//     REAL   y = 0.0;
//     REAL   y2 = 0.0;

//     for (int i=0;i<n;i++) {
//         x  += xList[i];
//         x2 += xList[i] * xList[i];
//         xy += xList[i] * yList[i];
//         y  += yList[i]; 
//         y2 += yList[i] * yList[i];
//     }

//     REAL denom = (n * x2 - x*x);

//     *m = (n * xy  -  x * y) / denom;
//     *b = (y * x2  -  x * xy) / denom;
// }


int main()
{
    // Compute: xsum and x2sum
    computeXSum();

    addValue(4);  // 0
    addValue(6);  // 1
    addValue(12); // 2
    addValue(34); // 3
    addValue(68); // 4

    // int n = 6;
    // REAL x[6]=   {0, 1,  2,  3,  4};
    // double y[n]= {4, 6, 12, 34, 68};

    double m;



    leastSquare(n, x, y, &m, &b);
    printf("m=%g b=%g\n",m,b);
    double result = leastSquare2(n, x, y);
    printf("My result: %g\n", result);
    return 0;
}
