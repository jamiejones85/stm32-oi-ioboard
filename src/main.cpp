/*
 * This file is part of the stm32-template project.
 *
 * Copyright (C) 2020 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/rtc.h>
#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/iwdg.h>
#include "stm32_can.h"
#include "canmap.h"
#include "cansdo.h"
#include "terminal.h"
#include "params.h"
#include "hwdefs.h"
#include "digio.h"
#include "hwinit.h"
#include "anain.h"
#include "param_save.h"
#include "my_math.h"
#include "errormessage.h"
#include "printf.h"
#include "stm32scheduler.h"
#include "terminalcommands.h"

#define PRINT_JSON 0

static Stm32Scheduler* scheduler;
static CanHardware* can;
static CanMap* canMap;

static void broadcastInputs(void) {
   uint8_t bytes[8];

   bytes[0] = 0x00;
   bytes[1] = 0x00;
   bytes[2] = 0x00;
   bytes[3] = 0x00;
   bytes[4] = 0x00;
   bytes[5] = 0x00;
   bytes[6] = 0x00;

   bytes[7] = 0x0 & DigIo::gp_in.Get() < 3 & DigIo::gp2_in.Get() < 2 & DigIo::notpark_in.Get() < 1 && DigIo::notpark_in.Get();

   can->Send(Param::GetInt(Param::inputid), (uint32_t*)bytes, 8);

}

//sample 100ms task
static void Ms100Task(void)
{
   //The following call toggles the LED output, so every 100ms
   //The LED changes from on to off and back.
   //Other calls:
   //DigIo::led_out.Set(); //turns LED on
   //DigIo::led_out.Clear(); //turns LED off
   //For every entry in digio_prj.h there is a member in DigIo
   DigIo::led_out.Toggle();
   //The boot loader enables the watchdog, we have to reset it
   //at least every 2s or otherwise the controller is hard reset.
   iwdg_reset();
   //Calculate CPU load. Don't be surprised if it is zero.
   float cpuLoad = scheduler->GetCpuLoad();
   //This sets a fixed point value WITHOUT calling the parm_Change() function
   Param::SetFloat(Param::cpuload, cpuLoad / 10);

   //If we chose to send CAN messages every 100 ms, do this here.
   if (Param::GetInt(Param::canperiod) == CAN_PERIOD_100MS) {
      canMap->SendAll();
      broadcastInputs();
   }
}

//sample 10 ms task
static void Ms10Task(void)
{
   //Set timestamp of error message
   ErrorMessage::SetTime(rtc_get_counter_val());

   //If we chose to send CAN messages every 10 ms, do this here.
   if (Param::GetInt(Param::canperiod) == CAN_PERIOD_10MS) {
      canMap->SendAll();
      broadcastInputs();
   }
}

/** This function is called when the user changes a parameter */
void Param::Change(Param::PARAM_NUM paramNum)
{
   switch (paramNum)
   {
      case Param::outputid:
        can->ClearUserMessages();
        break;
      
      default:
      //Handle general parameter changes here. Add paramNum labels for handling specific parameters
      break;
   }
}

//Whichever timer(s) you use for the scheduler, you have to
//implement their ISRs here and call into the respective scheduler
extern "C" void tim2_isr(void)
{
   scheduler->Run();
}

static bool CanCallback(uint32_t id, uint32_t data[2], uint8_t dlc) //This is where we go when a defined CAN message is received.
{
   dlc = dlc;
   uint8_t* bytes = (uint8_t*)data;

   if (id == Param::GetInt(Param::outputid)) {
      bytes[7] & 0b00000001 ? DigIo::gp_out.Set() : DigIo::gp_out.Clear();
      bytes[7] & 0b00000010 ? DigIo::gp2_out.Set() : DigIo::gp2_out.Clear();
      bytes[7] & 0b00000100 ? DigIo::a_out.Set() : DigIo::a_out.Clear();
      bytes[7] & 0b00001000 ? DigIo::b_out.Set() : DigIo::b_out.Clear();
      bytes[7] & 0b00010000 ? DigIo::c_out.Set() : DigIo::c_out.Clear();
      bytes[7] & 0b00100000 ? DigIo::d_out.Set() : DigIo::d_out.Clear();
      bytes[7] & 0b01000000 ? DigIo::e_out.Set() : DigIo::e_out.Clear();
      bytes[7] & 0b10000000 ? DigIo::parkrel_out.Set() : DigIo::parkrel_out.Clear();
      
      bytes[6] & 0b00000001 ? DigIo::parkhl_out.Set() : DigIo::parkhl_out.Clear();
      bytes[6] & 0b00000010 ? DigIo::line_out.Set() : DigIo::line_out.Clear();
      bytes[6] & 0b00000100 ? DigIo::tcc_out.Set() : DigIo::tcc_out.Clear();
   }

   return false;
}

//Whenever the user clears mapped can messages or changes the
//CAN interface of a device, this will be called by the CanHardware module
static void SetCanFilters()
{
   can->RegisterUserMessage(0x601); //CanSDO
   can->RegisterUserMessage(Param::GetInt(Param::outputid));

}

extern "C" int main(void)

{
   extern const TERM_CMD termCmds[];

   clock_setup(); //Must always come first
   rtc_setup();
   ANA_IN_CONFIGURE(ANA_IN_LIST);
   DIG_IO_CONFIGURE(DIG_IO_LIST);
   AnaIn::Start(); //Starts background ADC conversion via DMA

   tim_setup(); //Sample init of a timer
   nvic_setup(); //Set up some interrupts
   parm_load(); //Load stored parameters

   Stm32Scheduler s(TIM2); //We never exit main so it's ok to put it on stack
   scheduler = &s;
   //Initialize CAN1, including interrupts. Clock must be enabled in clock_setup()
   Stm32Can c(CAN1, (CanHardware::baudrates)Param::GetInt(Param::canspeed));
   FunctionPointerCallback cb(CanCallback, SetCanFilters);

   CanMap cm(&c);
   CanSdo sdo(&c, &cm);
   sdo.SetNodeId(Param::GetInt(Param::nodeid)); //Set node ID for SDO access e.g. by wifi module
   //store a pointer for easier access
   can = &c;
   canMap = &cm;
   
   c.AddCallback(&cb);

   //This is all we need to do to set up a terminal on USART3
   Terminal t(USART3, termCmds);
   TerminalCommands::SetCanMap(canMap);

   //Up to four tasks can be added to each timer scheduler
   //AddTask takes a function pointer and a calling interval in milliseconds.
   //The longest interval is 655ms due to hardware restrictions
   //You have to enable the interrupt (int this case for TIM2) in nvic_setup()
   //There you can also configure the priority of the scheduler over other interrupts
   s.AddTask(Ms10Task, 10);
   s.AddTask(Ms100Task, 100);

   //backward compatibility, version 4 was the first to support the "stream" command
   Param::SetInt(Param::version, 4);
   Param::Change(Param::PARAM_LAST); //Call callback one for general parameter propagation

   //
   can->ClearUserMessages();
   //Now all our main() does is running the terminal
   //All other processing takes place in the scheduler or other interrupt service routines
   //The terminal has lowest priority, so even loading it down heavily will not disturb
   //our more important processing routines.
   while(1)
   {
      char c = 0;
      t.Run();
      if (sdo.GetPrintRequest() == PRINT_JSON)
      {
         TerminalCommands::PrintParamsJson(&sdo, &c);
      }
   }


   return 0;
}

extern "C" void __cxa_pure_virtual() { while (1); }

