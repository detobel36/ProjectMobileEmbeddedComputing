
#include <stdlib.h>
#include <math.h>                           

#define REAL double



void leastSquare(int n, const REAL xList[], const REAL yList[], REAL* m, REAL* b){
    REAL   x = 0.0;                      
    REAL   x2 = 0.0;                     
    REAL   xy = 0.0;                     
    REAL   y = 0.0;                      
    REAL   y2 = 0.0;                     

    
    
    for (int i=0;i<n;i++){ 
        x  += xList[i];
        x2 += xList[i] * xList[i];  
        xy += xList[i] * yList[i];
        y  += yList[i]; 
        y2 += yList[i] * yList[i]; 
    } 

    REAL denom = (n * x2 - x*x);

    *m = (n * xy  -  x * y) / denom;
    *b = (y * x2  -  x * xy) / denom;
 
}


int main()
{
    int n = 6;
    REAL x[6]= {1, 2, 4,  5,  10, 20};
    REAL y[6]= {4, 6, 12, 15, 34, 68};

    REAL m,b;
    leastSquare(n,x,y,&m,&b);
    printf("m=%g b=%g\n",m,b);
    return 0;
}
