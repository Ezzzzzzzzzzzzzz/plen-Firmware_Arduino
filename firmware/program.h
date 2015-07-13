// =============================================================================
// StepXX：「かべにぶつからないようにしよう！」
// =============================================================================

#define UGOKI(bangou, kaisuu) Code::setCode(bangou, kaisuu);
#define UGOKENAI              Sensor::ODS().value()
delay(3000);

// =============================================================================
// 
// UGOKI(番号,回数)
// と入力すると、番号の動きを回数分くりかえすよ！
// 
// ====================== ここから先をかえてみてね↓ ===========================

while (true)
{
	if (UGOKENAI)
	{
		UGOKI(44, 1) //「止まる」を１回
	}
	else
	{
		UGOKI(70, 1) //「前へ進む」を１回
	}
}

// ======================= ここまでをかえてみてね↑ ============================

Code::run();