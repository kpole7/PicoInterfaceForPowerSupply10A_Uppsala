

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"


int main(){
    stdio_init_all();

	while(true)
	{

		printf("\r\nHello guys\r\n");

        sleep_ms(2000);
	}
}


