@echo off

SETX MWSDK_ROOT_WINNAME ""
REG DELETE HKCU\Environment /V MWSDK_ROOT_WINNAME /F

SETX MWSDK_ROOT ""
REG DELETE HKCU\Environment /V MWSDK_ROOT /F

echo _
echo ########################################################
echo ���ϐ� MWSDK_ROOT �� MWSDK_ROOT_WINNAME �𖕏����܂���
echo ########################################################
echo �m�F�̂��ߐݒ�_�C�A���O���J���܂�
echo �����L�[����͂��Ă�������...

PAUSE

rundll32 sysdm.cpl,EditEnvironmentVariables
