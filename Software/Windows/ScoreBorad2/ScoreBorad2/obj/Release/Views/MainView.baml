   M S B A M L   `   `   `  ���� F  BScoreBorad2, Version=1.0.0.0, Culture=neutral, PublicKeyToken=null?$clr-namespace:ScoreBorad2.ViewModelsScoreBorad2.ViewModels  5clr-namespace:ScoreBorad2.ViewsScoreBorad2.Views       ScoreBorad2.Views.MainView   \ XPresentationFramework, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35R NWindowsBase, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35W SPresentationCore, Version=4.0.0.0, Culture=neutral, PublicKeyToken=31bf3856ad364e35D 9http://schemas.microsoft.com/winfx/2006/xaml/presentation    R NSystem.Xaml, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089:x,http://schemas.microsoft.com/winfx/2006/xaml     @d2http://schemas.microsoft.com/expression/blend/2008     Jmc;http://schemas.openxmlformats.org/markup-compatibility/2006     3vm$clr-namespace:ScoreBorad2.ViewModels     1localclr-namespace:ScoreBorad2.Views       � Title$  MainView��$
��1080��$
��1920�� � WindowState$ 	Maximized=�$��60(� � DataContext 0  *ScoreBorad2.ViewModels.ScoreBoradViewModel   � 	Resources    buttonTemplate �� 
TargetType �� CornerRadius��%n   &	        �� " ��.R��� $��border��$��1q�
�������#��z��$ 10��._��� $��Center=�$��Center=�.���  � RowDefinitions �� $��2* ��� $��2* ��� $��1* � � ColumnDefinitions �� �� .+��� $��0��$��0��.����  �� Orientation$ Vertical=�$��Center=�.+��� $��Center=�$��50(����� *Block1.BlockName+�� $��Center=�$��#DSEG7 Classic)�$��80(����� *Block1.Score+�� $ 
Horizontal=� 	 �� 
TargetType
 �� Property �� Value��%.   '��       �� "	 ��.��� 
 � ��$	 200��.+��� $ Vertical=�.+��� $
��+100������ *Block1.Plus100+�� $
��-100������ *Block1.Minus100+�� $ Vertical=�.+��� $	��+10������ *Block1.Plus10+�� $	��-10������ *Block1.Minus10+�� $ Vertical=�.+��� $��+1������ *Block1.Plus1+�� $��-1������ *Block1.Minus1+�� $��0��$��1��.���� $ Vertical=�$��Center=�.+��� $��Center=�$��50(����� *Block2.BlockName+�� $��Center=�$��#DSEG7 Classic)�$��80(����� *Block2.Score+�� $ 
Horizontal=� ��%.   '��       �� "	 ��.��� 
 � ��$	 200��.+��� $ Vertical=�.+��� $
��+100������ *Block2.Plus100+�� $
��-100������ *Block2.Minus100+�� $ Vertical=�.+��� $	��+10������ *Block2.Plus10+�� $	��-10������ *Block2.Minus10+�� $ Vertical=�.+��� $��+1������ *Block2.Plus1+�� $��-1������ *Block2.Minus1+�� $��1��$��0��.���� $ Vertical=�$��Center=�.+��� $��Center=�$��50(����� *Block3.BlockName+�� $��Center=�$��#DSEG7 Classic)�$��80(����� *Block3.Score+�� $ 
Horizontal=� ��%.   '��       �� "	 ��.��� 
 � ��$	 200��.+��� $ Vertical=�.+��� $
��+100������ *Block3.Plus100+�� $
��-100������ *Block3.Minus100+�� $ Vertical=�.+��� $	��+10������ *Block3.Plus10+�� $	��-10������ *Block3.Minus10+�� $ Vertical=�.+��� $��+1������ *Block3.Plus1+�� $��-1������ *Block3.Minus1+�� $��1��$��1��.���� $ Vertical=�$��Center=�.+��� $��Center=�$��50(����� *Block4.BlockName+�� $��Center=�$��#DSEG7 Classic)�$��80(����� *Block4.Score+�� $ 
Horizontal=� ��%.   '��       �� "	 ��.��� 
 � ��$	 200��.+��� $ Vertical=�.+��� $
��+100������ *Block4.Plus100+�� $
��-100������ *Block4.Minus100+�� $ Vertical=�.+��� $	��+10������ *Block4.Plus10+�� $	��-10������ *Block4.Minus10+�� $ Vertical=�.+��� $��+1������ *Block4.Plus1+�� $��-1������ *Block4.Minus1+�� $��2��$��2��$��20q�.���� $ 
Horizontal=�.+��� $��点数を隠す��$��Center=�d� $��	100の桁��$	��250�� d� 	IsChecked �� *	Toggle100+d� $��10の桁��$	��250�� �� *
Toggle10+d� $��1の桁��$	��250�� �� *	Toggle1+�� $��送信��$��2��$��1��$	��400��$��Right=�$��20q����� *Send+