#include <stdlib.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK_FAILED %d\n",__LINE__);abort();} } while(0)
#include <stdio.h>
#include "../../display-bridge-prototype/display-quality.h"
int main(void){
 unsigned char normal[256],text[256];display_lut(normal,0);display_lut(text,1);
 CHECK(text[0]==0 && text[255]==255);
 for(unsigned i=0;i<256;i++){CHECK(normal[i]==i);CHECK(text[i]<=i);if(i)CHECK(text[i]>=text[i-1]);}
 for(unsigned i=17;i<256;i+=17)CHECK(text[i]>text[i-17]);
 CHECK(text[128]>=100 && text[128]<=108);
 puts("DISPLAY83_LUT_ENDPOINTS_MONOTONE_16_GRAY_LEVELS_AND_CLASSIC_IDENTITY_OK");return 0;
}
