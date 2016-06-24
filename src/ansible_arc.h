void set_mode_arc(void);

void handler_ArcFrontShort(s32 data);
void handler_ArcFrontLong(s32 data);

void handler_LevelsEnc(s32 data);
void handler_LevelsRefresh(s32 data);
void handler_LevelsKey(s32 data);
void handler_LevelsTr(s32 data);
void handler_LevelsTrNormal(s32 data);

void handler_CyclesEnc(s32 data);
void handler_CyclesRefresh(s32 data);
void handler_CyclesKey(s32 data);
void handler_CyclesTr(s32 data);
void handler_CyclesTrNormal(s32 data);