//---------------------------------------------------------------------------
#include "stdafx.h"

#pragma hdrstop

#include "ZernikePolynomial.h"
#include <math.h>
#include <assert.h>

	TZernikeNormType ZernikeNormType;


	double TZernikePolynomial::Fxy( double x, double y)
   {
		double r  = sqrt(x*x+y*y);
      double fi = (fabs(x)+fabs(y)<1e-10)?0:atan2(y,x);
      return Frad(r,fi);

   };
	double TZernikePolynomial::Frad( double r, double fi)
   {
     	int n;
     	int l;
      double sum=0;
      for (int s =0; s< Size(); s++)
      {
      	GetIJ(s,&n,&l);
      	sum += c(s)*Unl(n,l,r,fi);
      }
   	return sum;
   };

   double TZernikePolynomial::Norm(int r, TZernikeNormType ZernikeNormType1)
   {
	  int n,m;
      GetIJ(r,&n,&m);
	  int l= (n-2*m);
	  return Norm(n,l,ZernikeNormType1);
   }
   double TZernikePolynomial::Norm(int n, int l, TZernikeNormType ZernikeNormType1)
   {
	   if (ZernikeNormType == zntInvalid) 
		   ZernikeNormType1 = ZernikeNormType;

	   if (ZernikeNormType1 == zntNone)
		   return 1.0;
	   else if(ZernikeNormType1 == zntFirstTerm)
	   {
			int m=(n-l)/2;
			return 1./(factorial(n)/factorial(m)/factorial(n-m));
	   }
	   else if(ZernikeNormType1 == zntShareInRMS)
	   {
		   int m=(n-l)/2;
		   double e = (m==2*n)?2:1;
		   return 1./sqrt(e*n-1);
	   }
	   return 1.0;
   };
   char * TZernikePolynomial::FormulaNorm(int n, int l)
   {
	   static char str[200];str[0]='1';str[0]='\0';

	   if (ZernikeNormType == zntNone)
	   {
		   sprintf(str,"1");
	   }	
	   else if(ZernikeNormType == zntFirstTerm)
	   {
			int m=(n-l)/2;
			int norm =factorial(n)/factorial(m)/factorial(n-m);
			if (norm ==1) sprintf(str,"1");
			else		  sprintf(str,"1/%d",norm);	
	   }
	   else if(ZernikeNormType == zntShareInRMS)
	   {
			int m=(n-l)/2;
		   double e = (m==2*n)?2:1;
		   double norm = sqrt(e*(n-1));
		  // int det = e*(n-1);
		  // int offscope = 1;
		  // for (int i=0;i<)
		  // {
				//if((det%i)==0 && int(sqrt(0.5+i))*int(sqrt(0.5+i)) ==i )
				//{
				//	det/=i;
				//	offscope*=i;
				//}
		  // }
		   if (fabs(norm-1.)<1e-5)	sprintf(str,"1");
		   else						sprintf(str,"1/%f",norm);	
	   }
	   return str;
   };

	// Вычисление значения полинома Цернике
	double TZernikePolynomial::Unl(int n,int l, double r, double fi)
   {
   	if (!even(n+l) || abs(l)>n) return 0;
		double R = Norm(n,l)*Rnl(n,l,r); 		// радиальная часть
      return (l>0)? R*sin(fi*l):
             (l<0)? R*cos(fi*l):
                    R;
   }
	// Вычисление значения радиальной части полинома Цернике
	double TZernikePolynomial::Rnl(int n, int l, double r)
	{
      // вычисление радиального полинома
      // Формула из Малахары (A2.4)
   		int m=(n-abs(l))/2;
   		double sum =0;
      double r_pow = pow(r,n-2*m);
      double tmp = double(factorial(n-m))/factorial(m)/factorial(n-2*m);
      if (( m & 1)) tmp= tmp*(-1);
      for (int s = m;s>=0;s--)
      {
         sum += tmp*r_pow;

         r_pow *= r*r;
         tmp *= (-1.0)*(s)*G0(n-s+1)/G0(m-s+1)/G0(n-m-s+1);
      }
      return sum;
	};
	// Формула вычисления полинома Цернике
	char* TZernikePolynomial::Formula(int r)
	{
	  int n,m;
      GetIJ(r,&n,&m);
	  int l= (n-2*m);
	  return Formula(n,l);
	};
	// Формула (строка)
	char* TZernikePolynomial::Formula(int n,int l)
	{
   		static char str[1000];
		str[0]='\0';
		// радиальная часть
   		if (!even(n+l) || abs(l)>n) return str;
			char *R = FormulaRnl(n,l);

		char N[25];
		if (    fabs(Norm(n,l)-1.) <1e-5)      	sprintf(N,"");
		else 									sprintf(N,"%s * ",FormulaNorm(n,l));

		char bracket1[3];
		char bracket2[3];
   		if ((l!=0 || strlen(N)>1 ) && strlen(R)>3 )
		{
			sprintf(bracket1,"(");
			sprintf(bracket2,")");
		}
   		else
		{
			sprintf(bracket1,"");
			sprintf(bracket2,"");
		}

	
		// Угловая часть
		char f[10];
		char Q[20];
		if (l>0) 		sprintf(f,"sin");
		else 				sprintf(f,"cos");
      
		if (    l ==0)      	sprintf(Q,"");
		else if (abs(l)==1) 	sprintf(Q," * %s(A)",f);
		else 					sprintf(Q," * %s(%d*A)",f,abs(l));

		sprintf (str,"%s%s%s%s%s",N,bracket1,R,bracket2,Q);
		return str;
	};

	// Формула вычисления радиального полинома
	char* TZernikePolynomial::FormulaRnl(int n,int l)
	{
   	static char str[2000];
      str[0]='\0';
   	  //
      // вычисление радиального полинома
      // Формула из Малахары (A2.4)

   	char sign3[200];
   	char sign1[200];
   	char sign2[200];

   	int m= (n-abs(l))/2;
   	double sum =0;
//      double ro_pow = pow(ro,n-2*m);
 		int p = n-2*(m);
      double tmp1=0,tmp = double(factorial(n-m))/factorial(m)/factorial(n-2*m);
      if (( m & 1)) tmp= tmp*(-1);
      for (int s = m;s>=0;s--)
      {
//         ro_pow *= ro*ro;

         char term2[200];
         char term1[200];
         if (tmp==0) 						sprintf( term1, ""); 	// Коэфициент
         else if (abs((int)tmp)==1 && p==0) sprintf( term1, "1");
         else if (abs((int)tmp)==1)			sprintf( term1, "");
         else 									sprintf( term1, "%d",abs((int)tmp));

         if (tmp==0) 						sprintf( term2, "");    // Степень R
         else if				  	 (p==0)	sprintf( term2, "");
         else if				  	 (p==1)	sprintf( term2, "R");
         else                  		  	sprintf( term2, "R^%d", p);

         if (strcmp( term1,"")!=0 && strcmp( term2,"")!=0)
                                     sprintf( sign3, "*");		// Знак между коэфициентом и R в степени
         else                        sprintf( sign3, "");
         if (tmp<0)                  sprintf( sign1, " - ");   // Знак перед этим членом
         else                        sprintf( sign1, "");
         if (s!=0&&s!=m&&tmp1>0)     sprintf( sign2, " + ");   // Знак после
         else                        sprintf( sign2, "");

         char new_str[200];
         sprintf (new_str, "%s%s%s%s%s%s",sign1,term1,sign3,term2,sign2,str);
         sprintf (str, "%s",new_str);
         p = p+2;
         tmp1 = tmp;
         tmp *= (-1.0)*(s)*G0(n-s+1)/G0(m-s+1)/G0(n-m-s+1);
      }
      return str;
	};

	char* TZernikePolynomial::Meaning(int r)
	{
	  int n,m;
      GetIJ(r,&n,&m);
	  int l= (n-2*m);
	  return Meaning(n,l);
	};
	char * TZernikePolynomial::Meaning(int n, int l)
	{
   	static char* ZernikeMeaning[] = {
				/* 0, 0*/"constant",
				/* 1,-1*/"tilt at x",
				/* 1, 1*/"tilt at y",
				/* 2,-2*/"astigmatism at 45°",
				/* 2, 0*/"defocus",
				/* 2, 2*/"astigmatism at 0°",
				/* 3,-3*/"triangular astigmatism at x",
				/* 3,-1*/"3-order coma at x",
				/* 3, 1*/"3-order coma at y",
				/* 3, 3*/"triangular astigmatism at y",
				/* 4,-4*/"quaterfoil",
				/* 4,-2*/"4-order astigmatism ",
				/* 4, 0*/"4-order spherical aberration",
				/* 4, 2*/"4-order astigmatism ",
				/* 4, 4*/"quaterfoil" 
				};
     	int m= (n-l)/2;
      int r = GetIndex(n,m);

      return (r<sizeof(ZernikeMeaning)/sizeof(ZernikeMeaning[1]))?ZernikeMeaning[r]:" ";
	};
	char* TZernikePolynomial::Name(int r)
	{
	  int n,m;
      GetIJ(r,&n,&m);
	  int l= (n-2*m);
	  return Name(n,l);
	};
	char* TZernikePolynomial::Name(int n, int l)
	{
	   	static char str[200];
		sprintf (str, "z[%2d,%2d]",n,l);
		return str;
	};

   double& 	TZernikePolynomial::z( int n, int l )  // Доступ к коэфициентам через степень и частоту
	{                           // (это не тоже самое что по индексам)
   	assert(even(n+l) && abs(l)<=n);
     	int m= (n-l)/2;
      return c(n,m);
	};

   // Средне кваратичная деформация волновго фронта
   // см. Малахару. (A2.11)
   double TZernikePolynomial::Variance()

   {
   	int n,m;
      double e,v =0;
   	for (int r =0;r<Size();r++)
      {
         GetIJ(r,&n,&m);
         e = (n==2*m)?2:1;
         v += e/2/(n+1)*c(r)*c(r);
      }
      return v;
   };

   // Число штреля. - нормированная интенсивность в плскости Гауссса.
   // см. Малахару. (A2.12)
   double TZernikePolynomial::Shtrail(double lambda)

   {
      return 1.0-pow(3.14159265358979323846*2/lambda,2.0)*Variance();
   };


//---------------------------------------------------------------------------

#pragma package(smart_init)
