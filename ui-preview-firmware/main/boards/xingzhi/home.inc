// 240x240 layout; keep readable fonts instead of scaling the M5 canvas.
static void build_compact_home(void){
 home=box(lv_scr_act(),0,0,240,240,0x0b151c,0);
 home_label(FMO_BOARD_TITLE,8,5,65,&status_font,0xd4e4eb);
 local_call=home_label("--",70,5,90,&status_font,0x7ec9ff);lv_label_set_long_mode(local_call,LV_LABEL_LONG_SCROLL_CIRCULAR);
 net=home_label("Wi-Fi",164,5,32,&ui_small,0xb4c7d2);
 battery_label=home_label("--",198,5,42,&ui_small,0xb4c7d2);battery_fill=box(home,210,21,18,3,0x58d6b2,1);
 lv_obj_add_flag(battery_fill,LV_OBJ_FLAG_HIDDEN);
 box(home,6,27,228,91,0x14232d,10);
 caption=home_label("我的呼号",12,32,158,&ui_small,0xb4c7d2);
 badge=home_label("未接入",176,32,52,&ui_small,0xa8b5bf);
 call_label=home_label("--",12,55,216,&ui_call,0xffd166);
 hint=home_label("等待重新接入",12,100,158,&ui_small,0xc2d3de);
 for(int i=0;i<9;i++)bars[i]=box(home,180+i*5,110,3,3,0x58d6b2,1);
 home_label("上次",8,125,32,&status_font,0xb4c7d2);
 last_call=home_label("--",44,123,188,&font20,0x7ec9ff);
 previous_call=home_label("--",44,148,188,&history_font,0x7ec9ff);
 lv_label_set_long_mode(last_call,LV_LABEL_LONG_SCROLL_CIRCULAR);lv_label_set_long_mode(previous_call,LV_LABEL_LONG_SCROLL_CIRCULAR);
 relay_name=home_label("",8,174,174,&relay_font,0xe0ebf1);
 relay_id=home_label("",184,176,48,&status_font,0xb4c7d2);
 home_notice=home_label("",8,194,224,&ui_small,0xc2d3de);lv_label_set_long_mode(home_notice,LV_LABEL_LONG_SCROLL_CIRCULAR);
 box(home,0,215,240,25,0x101e28,0);
 home_label("A 音量-",4,221,67,&ui_small,0xd4e4eb);
 b_hint=home_label("B 开始发话",74,221,96,&ui_small,0x58d6b2);
 home_label("C 音量+",174,221,66,&ui_small,0xd4e4eb);
 lv_obj_add_flag(home,LV_OBJ_FLAG_HIDDEN);
}
