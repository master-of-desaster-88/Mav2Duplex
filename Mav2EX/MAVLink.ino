/*
	 Mav2DupEx Version 0.1
	 2014, by DevFor8.com, info@devfor8.com
	 
	 part of code is based on ArduCAM OSD

   2017, by Radek Voltr, voltr@voltr.eu
   removed FastSerial and few ArduCopter code dependencies
*/

#include "../GCS_MAVLink/include/mavlink/v1.0/mavlink_types.h"
#include "../GCS_MAVLink/include/mavlink/v1.0/ardupilotmega/mavlink.h"
#include "math.h"

#define ToDeg(x) (x*57.2957795131)  // *180/pi


// true when we have received at least 1 MAVLink packet
static bool mavlink_active;
static uint8_t crlf_count = 0;



boolean getBit(byte Reg, byte whichBit) {
    boolean State;
    State = Reg & (1 << whichBit);
    return State;
}  

#pragma pack(push,1)
typedef struct _Stream_params 
{
    uint8_t stream;
    uint8_t rate;
} Stream_params;

void request_mavlink_rates()
{
    
    static const Stream_params PROGMEM MAVStreams[] = 
    {
        {  MAV_DATA_STREAM_EXTENDED_STATUS, 4 },
        {  MAV_DATA_STREAM_EXTRA1,    4 },
        {  MAV_DATA_STREAM_EXTRA2,    4 }
    };


    for (uint8_t i=0; i < sizeof(MAVStreams)/sizeof(Stream_params); i++) 
    {
    uint8_t rate = pgm_read_byte(&(MAVStreams[i].rate));

        mavlink_msg_request_data_stream_send(MAVLINK_COMM_0,
            mavlink_system.sysid , mavlink_system.compid ,  // received by HEARTBEAT 
            pgm_read_byte(&(MAVStreams[i].stream)),
            rate, 1);
    }
}


    mavlink_status_t status;
    mavlink_message_t* msg = (mavlink_message_t*)malloc(MAVLINK_MAX_PACKET_LEN);
    
void read_mavlink(int maxframes)
{
    int current_frames = 0;
     
      if (enable_mav_request)
      {
        if(!mav_request_done){ // we got HEARTBEAT packet and still don't send requests
        for(byte n = 3; n >0; n--){
            request_mavlink_rates();//Three times to certify it will be readed
            delay(150);
            }
        mav_request_done=1;
        //Serial.println("Data requested");
        }
      }


    //grabing data 
    while(mavlink_comm_0_port->available() > 0) 
    { 
        uint8_t c = mavlink_comm_0_port->read();

        //trying to grab msg  
        if(mavlink_parse_char(MAVLINK_COMM_0, c, msg, &status)) 
        {
            mavlink_active = 1;
            current_frames++; //this is frame

            if (mav_request_done == 0)
            {
                    //Serial.println("first message arrived");
                    apm_mav_system    = msg->sysid;
                    apm_mav_component = msg->compid;

                    mavlink_system.sysid = msg->sysid;
                    mavlink_system.compid = msg->compid;

                    if(waitingMAVBeats == 1)
                    {
                        enable_mav_request = 1;
                    }
            }

                  #ifdef DEBUG1f
                  Serial.print(msg->msgid);Serial.print(" ");
                  #endif
                              
            //handle msg
            switch(msg->msgid) 
            {
            case MAVLINK_MSG_ID_HEARTBEAT:
                {
                  #ifdef DEBUG1
                  Serial.println("beat");
                  #endif
                    mavbeat = 1;
                    apm_mav_system    = msg->sysid;
                    apm_mav_component = msg->compid;

                    mavlink_system.sysid = msg->sysid;
                    mavlink_system.compid = msg->compid;
                    
                    apm_mav_type      = mavlink_msg_heartbeat_get_type(msg);            
                 //   osd_mode = mavlink_msg_heartbeat_get_custom_mode(msg);
                    osd_mode = (uint8_t)mavlink_msg_heartbeat_get_custom_mode(msg);
                    //Mode (arducoper armed/disarmed)
                    base_mode = mavlink_msg_heartbeat_get_base_mode(msg);
                    if(getBit(base_mode,7)) motor_armed = 1;
                    else motor_armed = 0;

                    lastMAVBeat = millis();
                    if(waitingMAVBeats == 1)
                    {
                        enable_mav_request = 1;
                    }
                }
                break;
            case MAVLINK_MSG_ID_STATUSTEXT:
                {
                  //Serial.println("stext");
                  char text[MAVLINK_MSG_ID_STATUSTEXT_LEN] ;
                  mavlink_msg_statustext_get_text(msg,&text[0]);
                  strncpy((char*)LastMessage,text,LCDMaxPos);
                  break;
                }
            case MAVLINK_MSG_ID_BATTERY_STATUS:
            {
                osd_current_consumed = (mavlink_msg_battery_status_get_current_consumed(msg)); //Battery current consumed mA
            }
            break;

            case MAVLINK_MSG_ID_SCALED_PRESSURE: 
            {
                osd_press_abs = (mavlink_msg_scaled_pressure_get_press_abs(msg)); // Barometrischer Druck vor Ort
                osd_press_alt = 44330 * (1 - pow((float)(osd_press_abs / 1013.25),(float)(1 / 5.255)));  // Barometrische H�he in m 
            }
            break;

            case MAVLINK_MSG_ID_SYS_STATUS:
                {
                  //Serial.println("status");
                    osd_vbat_A = (mavlink_msg_sys_status_get_voltage_battery(msg)/100); //Battery voltage, in millivolts (1 = 1 millivolt)
                    osd_curr_A = (mavlink_msg_sys_status_get_current_battery(msg)/10); //Battery current, in 10*milliamperes (1 = 10 milliampere)
                    osd_battery_remaining_A = mavlink_msg_sys_status_get_battery_remaining(msg); //Remaining battery energy: (0%: 0, 100%: 100)
                }
                break;

            case MAVLINK_MSG_ID_GPS_RAW_INT:
                {
                  //Serial.print("raw:");
                  // fix by rosewhite
                   osd_lat_org = mavlink_msg_gps_raw_int_get_lat(msg); // store the orignal data
                   osd_lon_org = mavlink_msg_gps_raw_int_get_lon(msg);
                   osd_lat = osd_lat_org / 10000000.0f;
                   osd_lon = osd_lon_org / 10000000.0f;
                   osd_fix_type = mavlink_msg_gps_raw_int_get_fix_type(msg); // 0 = No GPS, 1 =No Fix, 2 = 2D Fix, 3 = 3D Fix
                   osd_satellites_visible = mavlink_msg_gps_raw_int_get_satellites_visible(msg);
                   ap_gps_hdop = mavlink_msg_gps_raw_int_get_eph(msg); //in centimeters so multiplication not needed

                }
                break; 
            case MAVLINK_MSG_ID_VFR_HUD:
                {
                  //Serial.print("hud:");
                    osd_airspeed = (mavlink_msg_vfr_hud_get_airspeed(msg)*3.6); // in kmh
                    osd_groundspeed = (mavlink_msg_vfr_hud_get_groundspeed(msg)*3.6); // in kmh
                    osd_heading = mavlink_msg_vfr_hud_get_heading(msg); // 0..360 deg, 0=north
                  //Serial.println(osd_heading);  
                    osd_throttle = mavlink_msg_vfr_hud_get_throttle(msg);
                    osd_alt = mavlink_msg_vfr_hud_get_alt(msg);
                    osd_climb = mavlink_msg_vfr_hud_get_climb(msg);
                }
                break;
            case MAVLINK_MSG_ID_ATTITUDE:
                {
                  //Serial.println("atti");
                    osd_pitch = ToDeg(mavlink_msg_attitude_get_pitch(msg));
                    osd_roll = ToDeg(mavlink_msg_attitude_get_roll(msg));
                    osd_yaw = ToDeg(mavlink_msg_attitude_get_yaw(msg));
            #ifdef DEBUG
                    Serial.print(millis());  Serial.print(" : "); Serial.print("osd_press_abs"); Serial.print("  / "); Serial.print(osd_press_abs); Serial.print("  / "); Serial.print("osd_press_alt"); Serial.print(" / ");
                    Serial.print(osd_press_alt); Serial.print(" / "); Serial.print("osd_home_altdif"); Serial.print(" / "); Serial.println(osd_home_altdif);
            #endif
                }
                break;

            default:
                {
                    
                   //Serial.print("unk:");Serial.println(msg->msgid);
                }
                break;
            }
        }
        //next one
        if (current_frames>maxframes)
          {
            //Serial.println("MaxFrames reached");
            break; //we need time for Jeti
          }
    }
    // Update global packet drops counter
}
