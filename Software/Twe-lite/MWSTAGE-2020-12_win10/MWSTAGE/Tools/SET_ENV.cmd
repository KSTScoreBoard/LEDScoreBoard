@echo off

rem GET CURRENT DIR
cd /D %~dp0%..\MWSDK
SET WD=%CD%\

SET WDS=%WD:\=/%

rem CHECK CURRENT DIR IF INCLUDING SPACE or NOT.
echo %WDS: =*% | find "*" > NUL
IF %ERRORLEVEL% == 0 GOTO ERRSPC

:OKAY
SETX MWSDK_ROOT %WDS%
SETX MWSDK_ROOT_WINNAME %WD% 

echo _
echo ########################################################
echo ���ϐ� MWSDK_ROOT �� MWSDK_ROOT_WINNAME ��o�^���܂���
echo ########################################################
echo �m�F�̂��ߊ��ϐ��ݒ�_�C�A���O���J���܂�
echo �����L�[����͂��ĉ�����...

PAUSE
rundll32 sysdm.cpl,EditEnvironmentVariables

exit 0

:ERRSPC
CLS

echo _
echo ################################################################
echo !!! �G���[ !!!
echo MWSDK �̃C���X�g�[���f�B���N�g���ɋ󔒕������܂܂�Ă��܂��B
echo �󔒕�������{�ꕶ�����܂܂�Ȃ��f�B���N�g���Ɉړ����Ă��������B
echo ################################################################
echo �I�����܂��B
echo �����L�[����͂��ĉ�����...

PAUSE