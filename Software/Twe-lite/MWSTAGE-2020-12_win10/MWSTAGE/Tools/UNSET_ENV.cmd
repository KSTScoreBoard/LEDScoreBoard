@echo off

SETX MWSDK_ROOT_WINNAME ""
REG DELETE HKCU\Environment /V MWSDK_ROOT_WINNAME /F

SETX MWSDK_ROOT ""
REG DELETE HKCU\Environment /V MWSDK_ROOT /F

echo _
echo ########################################################
echo 環境変数 MWSDK_ROOT と MWSDK_ROOT_WINNAME を抹消しました
echo ########################################################
echo 確認のため設定ダイアログを開きます
echo 何かキーを入力してください...

PAUSE

rundll32 sysdm.cpl,EditEnvironmentVariables
