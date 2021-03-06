PULSONIX_LIBRARY_ASCII "SamacSys ECAD Model"
//1171825/128208/2.46/12/4/Display

(asciiHeader
	(fileUnits MM)
)
(library Library_1
	(padStyleDef "c105_h65"
		(holeDiam 0.65)
		(padShape (layerNumRef 1) (padShapeType Ellipse)  (shapeWidth 1.05) (shapeHeight 1.05))
		(padShape (layerNumRef 16) (padShapeType Ellipse)  (shapeWidth 1.05) (shapeHeight 1.05))
	)
	(padStyleDef "s105_h65"
		(holeDiam 0.65)
		(padShape (layerNumRef 1) (padShapeType Rect)  (shapeWidth 1.05) (shapeHeight 1.05))
		(padShape (layerNumRef 16) (padShapeType Rect)  (shapeWidth 1.05) (shapeHeight 1.05))
	)
	(textStyleDef "Normal"
		(font
			(fontType Stroke)
			(fontFace "Helvetica")
			(fontHeight 1.27)
			(strokeWidth 0.127)
		)
	)
	(patternDef "DIPS762W45P254L2253H610Q12N" (originalName "DIPS762W45P254L2253H610Q12N")
		(multiLayer
			(pad (padNum 1) (padStyleRef s105_h65) (pt 0, 0) (rotation 90))
			(pad (padNum 2) (padStyleRef c105_h65) (pt 0, -2.54) (rotation 90))
			(pad (padNum 3) (padStyleRef c105_h65) (pt 0, -5.08) (rotation 90))
			(pad (padNum 4) (padStyleRef c105_h65) (pt 0, -7.62) (rotation 90))
			(pad (padNum 5) (padStyleRef c105_h65) (pt 0, -10.16) (rotation 90))
			(pad (padNum 6) (padStyleRef c105_h65) (pt 0, -12.7) (rotation 90))
			(pad (padNum 7) (padStyleRef c105_h65) (pt 7.62, -12.7) (rotation 90))
			(pad (padNum 8) (padStyleRef c105_h65) (pt 7.62, -10.16) (rotation 90))
			(pad (padNum 9) (padStyleRef c105_h65) (pt 7.62, -7.62) (rotation 90))
			(pad (padNum 10) (padStyleRef c105_h65) (pt 7.62, -5.08) (rotation 90))
			(pad (padNum 11) (padStyleRef c105_h65) (pt 7.62, -2.54) (rotation 90))
			(pad (padNum 12) (padStyleRef c105_h65) (pt 7.62, 0) (rotation 90))
		)
		(layerContents (layerNumRef 18)
			(attr "RefDes" "RefDes" (pt 0, 0) (textStyleRef "Normal") (isVisible True))
		)
		(layerContents (layerNumRef Courtyard_Top)
			(line (pt -1.44 5.165) (pt 9.06 5.165) (width 0.05))
		)
		(layerContents (layerNumRef Courtyard_Top)
			(line (pt 9.06 5.165) (pt 9.06 -17.865) (width 0.05))
		)
		(layerContents (layerNumRef Courtyard_Top)
			(line (pt 9.06 -17.865) (pt -1.44 -17.865) (width 0.05))
		)
		(layerContents (layerNumRef Courtyard_Top)
			(line (pt -1.44 -17.865) (pt -1.44 5.165) (width 0.05))
		)
		(layerContents (layerNumRef 28)
			(line (pt -1.19 4.915) (pt 8.81 4.915) (width 0.025))
		)
		(layerContents (layerNumRef 28)
			(line (pt 8.81 4.915) (pt 8.81 -17.615) (width 0.025))
		)
		(layerContents (layerNumRef 28)
			(line (pt 8.81 -17.615) (pt -1.19 -17.615) (width 0.025))
		)
		(layerContents (layerNumRef 28)
			(line (pt -1.19 -17.615) (pt -1.19 4.915) (width 0.025))
		)
		(layerContents (layerNumRef 28)
			(line (pt -1.19 3.645) (pt 0.08 4.915) (width 0.025))
		)
		(layerContents (layerNumRef 18)
			(line (pt -1.19 -17.615) (pt 8.81 -17.615) (width 0.2))
		)
		(layerContents (layerNumRef 18)
			(line (pt 8.81 4.915) (pt -1.19 4.915) (width 0.2))
		)
		(layerContents (layerNumRef 18)
			(line (pt -1.19 4.915) (pt -1.19 0) (width 0.2))
		)
	)
	(symbolDef "2831BS-RIGHT-ANGLE" (originalName "2831BS-RIGHT-ANGLE")

		(pin (pinNum 1) (pt 0 mils 0 mils) (rotation 0) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 230 mils -25 mils) (rotation 0]) (justify "Left") (textStyleRef "Normal"))
		))
		(pin (pinNum 2) (pt 0 mils -100 mils) (rotation 0) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 230 mils -125 mils) (rotation 0]) (justify "Left") (textStyleRef "Normal"))
		))
		(pin (pinNum 3) (pt 0 mils -200 mils) (rotation 0) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 230 mils -225 mils) (rotation 0]) (justify "Left") (textStyleRef "Normal"))
		))
		(pin (pinNum 4) (pt 0 mils -300 mils) (rotation 0) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 230 mils -325 mils) (rotation 0]) (justify "Left") (textStyleRef "Normal"))
		))
		(pin (pinNum 5) (pt 0 mils -400 mils) (rotation 0) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 230 mils -425 mils) (rotation 0]) (justify "Left") (textStyleRef "Normal"))
		))
		(pin (pinNum 6) (pt 0 mils -500 mils) (rotation 0) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 230 mils -525 mils) (rotation 0]) (justify "Left") (textStyleRef "Normal"))
		))
		(pin (pinNum 7) (pt 2100 mils 0 mils) (rotation 180) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 1870 mils -25 mils) (rotation 0]) (justify "Right") (textStyleRef "Normal"))
		))
		(pin (pinNum 8) (pt 2100 mils -100 mils) (rotation 180) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 1870 mils -125 mils) (rotation 0]) (justify "Right") (textStyleRef "Normal"))
		))
		(pin (pinNum 9) (pt 2100 mils -200 mils) (rotation 180) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 1870 mils -225 mils) (rotation 0]) (justify "Right") (textStyleRef "Normal"))
		))
		(pin (pinNum 10) (pt 2100 mils -300 mils) (rotation 180) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 1870 mils -325 mils) (rotation 0]) (justify "Right") (textStyleRef "Normal"))
		))
		(pin (pinNum 11) (pt 2100 mils -400 mils) (rotation 180) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 1870 mils -425 mils) (rotation 0]) (justify "Right") (textStyleRef "Normal"))
		))
		(pin (pinNum 12) (pt 2100 mils -500 mils) (rotation 180) (pinLength 200 mils) (pinDisplay (dispPinName true)) (pinName (text (pt 1870 mils -525 mils) (rotation 0]) (justify "Right") (textStyleRef "Normal"))
		))
		(line (pt 200 mils 100 mils) (pt 1900 mils 100 mils) (width 6 mils))
		(line (pt 1900 mils 100 mils) (pt 1900 mils -600 mils) (width 6 mils))
		(line (pt 1900 mils -600 mils) (pt 200 mils -600 mils) (width 6 mils))
		(line (pt 200 mils -600 mils) (pt 200 mils 100 mils) (width 6 mils))
		(attr "RefDes" "RefDes" (pt 1950 mils 300 mils) (justify Left) (isVisible True) (textStyleRef "Normal"))
		(attr "Type" "Type" (pt 1950 mils 200 mils) (justify Left) (isVisible True) (textStyleRef "Normal"))

	)
	(compDef "2831BS-RIGHT-ANGLE" (originalName "2831BS-RIGHT-ANGLE") (compHeader (numPins 12) (numParts 1) (refDesPrefix DS)
		)
		(compPin "1" (pinName "CATHODE E") (partNum 1) (symPinNum 1) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "2" (pinName "CATHODE D") (partNum 1) (symPinNum 2) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "3" (pinName "CATHODE DP") (partNum 1) (symPinNum 3) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "4" (pinName "CATHODE C") (partNum 1) (symPinNum 4) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "5" (pinName "CATHODE G") (partNum 1) (symPinNum 5) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "6" (pinName "NC") (partNum 1) (symPinNum 6) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "12" (pinName "COMMON ANODE DIG 1") (partNum 1) (symPinNum 7) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "11" (pinName "CATHODE A") (partNum 1) (symPinNum 8) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "10" (pinName "CATHODE F") (partNum 1) (symPinNum 9) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "9" (pinName "COMMON ANODE DIG 2") (partNum 1) (symPinNum 10) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "8" (pinName "COMMON ANODE DIG 3") (partNum 1) (symPinNum 11) (gateEq 0) (pinEq 0) (pinType Unknown))
		(compPin "7" (pinName "CATHODE B") (partNum 1) (symPinNum 12) (gateEq 0) (pinEq 0) (pinType Unknown))
		(attachedSymbol (partNum 1) (altType Normal) (symbolName "2831BS-RIGHT-ANGLE"))
		(attachedPattern (patternNum 1) (patternName "DIPS762W45P254L2253H610Q12N")
			(numPads 12)
			(padPinMap
				(padNum 1) (compPinRef "1")
				(padNum 2) (compPinRef "2")
				(padNum 3) (compPinRef "3")
				(padNum 4) (compPinRef "4")
				(padNum 5) (compPinRef "5")
				(padNum 6) (compPinRef "6")
				(padNum 7) (compPinRef "7")
				(padNum 8) (compPinRef "8")
				(padNum 9) (compPinRef "9")
				(padNum 10) (compPinRef "10")
				(padNum 11) (compPinRef "11")
				(padNum 12) (compPinRef "12")
			)
		)
		(attr "Manufacturer_Name" "Shen Zhen Tekivi")
		(attr "Manufacturer_Part_Number" "2831BS-RIGHT-ANGLE")
		(attr "Mouser Part Number" "")
		(attr "Mouser Price/Stock" "")
		(attr "RS Part Number" "")
		(attr "RS Price/Stock" "")
		(attr "Description" "Small 3-digit red 7-segment LED display 7 mm high anode common anode common connection")
		(attr "<Hyperlink>" "http://akizukidenshi.com/download/ds/tekivi/2xx1BS_sq.pdf")
		(attr "<Component Height>" "6.1")
	)

)
