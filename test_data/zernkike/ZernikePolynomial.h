//---------------------------------------------------------------------------

#ifndef _ZernikePolynomial_H_
#define _ZernikePolynomial_H_

#include "SetOfCoeffs.h"

// различные виды нормировки 
enum TZernikeNormType
      {
	   zntInvalid = -1,
	   zntNone = 0,		 // Нет нормировки. (так как в борне и вольфе и в малакаре)
       zntFirstTerm, // Нормировка сделана так чтобы при максимальной степени был коэф. единица.
	   zntShareInRMS // Нормировка по вкладу в рмс
	  };

    //Тип нормировки полиномов цернике (устанавливается глобально)
extern	TZernikeNormType ZernikeNormType;


class TZernikePolynomial : public TSetOfCoeffs<double>
{
public:
	double Fxy( double x, double y);
	double Frad( double r, double fi);
    double Norm(int n, int l, TZernikeNormType ZernikeNormType1 = zntInvalid );
    double Norm(int r, TZernikeNormType ZernikeNormType1 = zntInvalid );


	// Вычисление значения всего полинома Цернике
	double Unl(int n,int l, double r, double fi);
	// Вычисление значения радиальной части полинома Цернике
	double Rnl(int n, int l, double r);

	// Формула вычисления полинома Цернике
	char * Formula(int n,int l);
	char * Formula(int r);
	// Формула вычисления радиального полинома
    char * FormulaNorm(int n, int l);
	char * FormulaRnl(int n,int l);
	char * Meaning(int n, int l);
	char * Meaning(int r);
	char * Name(int n, int l);
	char * Name(int r);

   double& 	z( int n, int l );  // Доступ к коэфициентам через степень и частоту

   // Средне кваратичная деформация волновго фронта
   // см. Малахару. (A2.11)
   double Variance();

   // Число штреля. - нормированная интенсивность в плскости Гауссса.
   // см. Малахару. (A2.12)
   double Shtrail(double lambda);
};

//---------------------------------------------------------------------------
#endif
