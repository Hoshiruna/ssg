/*
 *   Replay and practice menu flow
 */

#pragma once

struct tagWINDOW_SYSTEM;

void ReplayUIOpenLoad(void);
void ReplayUIOpenPractice(void);
void ReplayUIOpenSave(void (*done)(void));
void ReplayUIShowMessage(const char* message);

bool ReplayUIActive(void);
tagWINDOW_SYSTEM* ReplayUIWindow(void);
void ReplayUIMove(void);
void ReplayUIDraw(void);
