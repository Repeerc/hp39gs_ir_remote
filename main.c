/*
    Copyright (C) 2023 by Repeerc

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include <hpgcc49.h> 
#include <stdint.h> 
 
int bsz = 511;
uint32_t *pas;

volatile uint32_t *ir_en_port = (volatile uint32_t *)0x07A00030; 
volatile uint32_t *ir_dat_port = (volatile uint32_t *)0x07A00070; 

uint32_t modulating_cycle = 9;

void volatile send(int samps)
{
	uint32_t i = 0;
	uint32_t ticks = 0;
	uint32_t curtick = 0;
	uint32_t state = 1; 
	ir_dat_port[0] = 1 << 12; 
	if(!samps)
	{
		return;
	}
	while(!keyb_isON())
	{
		curtick++; 
		if(state){
			ticks++;
			if(ticks == modulating_cycle){
				ir_dat_port[1] |=  (1<< 6);
			}else if(ticks == (2 * modulating_cycle)) {
				ir_dat_port[1] &=  ~(1<< 6);
				ticks = 0;
			}
		}else{
			ticks++;
			if(ticks == modulating_cycle){
				ir_dat_port[1] &=  ~(1<< 6);
			}else if(ticks == (2 * modulating_cycle)) {
				ir_dat_port[1] &=  ~(1<< 6);
				ticks = 0;
			}
		}
		if(curtick >= (pas[i+1] - pas[i]) /5 * 4 )
		{
			curtick = 0;
			i++;
			state = !state;
			if(i == samps)
			{
				printf("Send Done\r\n");
				ir_dat_port[1] &=  ~(1<< 6);
				break;
			}
		} 
	} 
	sys_sleep(80);
}

uint32_t volatile record()
{
	volatile uint32_t sa = 0;
	volatile uint32_t sb = 0;	 
	volatile int i = 0;
	volatile int start = 0;  
	uint32_t curtick = 0;
	uint32_t lasttick = 0;
	ir_dat_port[0] = 0;
	ir_dat_port[2] = 0x7FF;
	

	sb = ir_dat_port[1] &  (1<< 7);
	while(!keyb_isON())
	//while(!keyb_isAnyKeyPressed())
	{
		sa = ir_dat_port[1] &  (1<< 7);
		if(start)
			curtick++;
		if(sa != sb)
		{
			if(start == 0)
			{
				start = 1;
				pas[0] = 0;
				i++;
			}
			if(curtick - lasttick < 50)
			{
				if(i < bsz){
					pas[i] = curtick;
				}
			}else{
				i++;
				if(i >= bsz){
					pas[i-1] = curtick;
					return -1;
					break;
				}
				pas[i] = curtick;
				i++;
				if(i >= bsz){
					pas[i-1] = curtick;
					return -1;
					break;
				}
			}
			lasttick = curtick;
			sb = sa;
		} 
	} 
	return i;
}

void ir_init()
{
	ir_en_port[1] &= ~(1 << 2);	
	ir_dat_port[0] = 0;
	ir_dat_port[2] = 0x7FF;
}

int select_slot = 0;
uint32_t sat_addrs = 0;

uint32_t get_slot_samples(uint32_t s)
{
	uint32_t off = sat_addrs + ((bsz + 1) * s) * 8;
	uint32_t nums = sat_peek(off, 8);
	return nums > bsz ? 0 : nums;
}


void draw_main()
{
	clear_screen(); 
	gotoxy(0,0);
	printf("== A Simple IR Remote Control ==\r\n");
	printf("F1: Send   F2: Record  ON: Exit\r\n");
	
	gotoxy(0,3);
	printf("SELECT:[%d]\r\n", select_slot);
	printf("Modulating Cycle:%d [+/-]\r\n", modulating_cycle);
	
	gotoxy(0,6);
	printf("SLOT [0] samples:%d\r\n", get_slot_samples(0));
	printf("SLOT [1] samples:%d\r\n", get_slot_samples(1));
	printf("SLOT [2] samples:%d\r\n", get_slot_samples(2));
	printf("SLOT [3] samples:%d", get_slot_samples(3));
	
}

void do_rec()
{
	clear_screen(); 
	memset(pas, 0, bsz);
	printf("Select Slot:%d \r\n", select_slot);
	printf("Receiving...\r\nPress [ON] to finish..\r\n");
	sys_sleep(500);
	
	uint32_t cnt = record();
	if(cnt == -1)
	{
		printf("ERROR: Overflow!!\r\n");
	 	while(!keyb_isON()); 
		sys_sleep(200);
		return ;
	}
	printf("Receive samples:%d\r\n", cnt);
	sys_sleep(200);
	printf("[Enter]: Save.\r\n");
	printf("[ON]: Return.\r\n");
	while(1)
	{
		while(!keyb_isAnyKeyPressed());
		if(keyb_isKeyPressed(0,6)) //Enter
		{
			uint32_t off = sat_addrs + ((bsz + 1) * select_slot) * 8;
			printf("Saving...\r\n");
			sat_poke(off, cnt, 8);
			off+=8;
			for(int i = 0; i < bsz; i++){
				sat_poke(off, pas[i], 8);
				off+=8;
			} 
			break;
		}
		if(keyb_isON())
		{
			break;
		}
	}
}

void do_send(int select_slot)
{
	clear_screen(); 
	memset(pas, 0, bsz);
	printf("Select Slot:%d \r\n", select_slot);
	printf("Loading...\r\n");
	uint32_t off = sat_addrs + ((bsz + 1) * select_slot) * 8;
	uint32_t nums = sat_peek(off, 8);
	if(!nums)
	{
		return;
	}
	off+=8;
	for(int i = 0; i < bsz; i++){
		pas[i] = sat_peek(off, 8);
		off+=8;
	} 
	printf("Sending...\r\n");
	printf("[ON]: Interrupt.\r\n");
	send(nums > bsz ? 0 : nums);
}

int sat_objlen(unsigned addr)
{
	return ((int)sat_peek_sat_addr(addr + 5) - 5) / 2;
}

int main(void){
	int dasz;
	
	clear_screen(); //clear the screen
	sys_slowOff();
	
	SAT_DIR_ENTRY *dir;
	
	__sat_cwd = _sat_find_path("/'notesdir");
	if (!__sat_cwd) {
		__sat_cwd = __sat_root;
	}
	dir = __sat_cwd->object;
	SAT_OBJ_DSCR *obj;
	while(dir)
	{
		obj = dir->sat_obj;
		if(strcmp(obj->name, "\'IRDATA") == 0)
		{
			goto irdata_found;
		}
		dir = dir->next;
	}
	printf("IRDATA Not Found!\r\n");
	while(!keyb_isON());
	sys_sleep(300);
		goto fin;
	
	irdata_found:
	
	dasz = sat_objlen(obj->addr);
	//printf("sz:%d\r\n",dasz);
	if(dasz != 8192)
	{
		printf("IRDATA Size ERROR!\r\n");
		while(!keyb_isON());
		sys_sleep(300);
			goto fin;
	}
	
	sat_addrs = obj->addr + 10;
	
	
	
	//while(!keyb_isON());
	//sys_sleep(300);
	
	ir_init();
	
	pas = calloc(4,bsz );
	if(!pas)
	{
		printf("malloc error!\r\n");
		goto fin;
	} 
	while(1)
	{
		draw_main();
		while(!keyb_isAnyKeyPressed());
		if(keyb_isKeyPressed(3,6)) //0
		{
			select_slot = 0;
			do_send(select_slot);
			while(keyb_isKeyPressed(3,6));
		}
		if(keyb_isKeyPressed(3,5)) //1
		{
			select_slot = 1;
			do_send(select_slot);
			while(keyb_isKeyPressed(3,5));
		}
		if(keyb_isKeyPressed(2,5)) //2
		{
			select_slot = 2;
			do_send(select_slot);
			while(keyb_isKeyPressed(2,5));
		}
		if(keyb_isKeyPressed(1,5)) //3
		{
			select_slot = 3;
			do_send(select_slot);
			while(keyb_isKeyPressed(1,5));
		}
		
		if(keyb_isKeyPressed(5,0)) //F1
		{
			do_send(select_slot);
			sys_sleep(300);
		}
		if(keyb_isKeyPressed(5,1)) //F2
		{
			do_rec();
			sys_sleep(300);
		}
		
		if(keyb_isKeyPressed(0,5)) //+
		{
			modulating_cycle++;
			if(modulating_cycle > 50)
			{
				modulating_cycle = 0;
			}
			sys_sleep(40);
		}
		if(keyb_isKeyPressed(0,4)) //-
		{
			modulating_cycle--;
			if(modulating_cycle <= 1)
			{
				modulating_cycle = 1;
			}
			sys_sleep(40);
		}
		
		if(keyb_isON())
		{
			break;
		}
		sys_sleep(100);
	}
	
	free(pas);
	fin:
	while(!keyb_isON());
}
