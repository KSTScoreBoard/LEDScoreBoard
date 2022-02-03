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
echo 環境変数 MWSDK_ROOT と MWSDK_ROOT_WINNAME を登録しました
echo ########################################################
echo 確認のため環境変数設定ダイアログを開きます
echo 何かキーを入力して下さい...

PAUSE
rundll32 sysdm.cpl,EditEnvironmentVariables

exit 0

:ERRSPC
CLS

echo _
echo ################################################################
echo !!! エラー !!!
echo MWSDK のインストールディレクトリに空白文字が含まれています。
echo 空白文字や日本語文字が含まれないディレクトリに移動してください。
echo ################################################################
echo 終了します。
echo 何かキーを入力して下さい...

PAUSE