#ifndef Pkcs7Contents_hxx
#define Pkcs7Contents_hxx

#include "sip2/sipstack/Contents.hxx"
#include "sip2/util/Data.hxx"

namespace Vocal2
{

class Pkcs7Contents : public Contents
{
   public:
      Pkcs7Contents();
      Pkcs7Contents(const Data& text);
      Pkcs7Contents(HeaderFieldValue* hfv, const Mime& contentType);
      Pkcs7Contents(const Data& data, const Mime& contentType);
      Pkcs7Contents(const Pkcs7Contents& rhs);
      virtual ~Pkcs7Contents();
      Pkcs7Contents& operator=(const Pkcs7Contents& rhs);

      virtual Contents* clone() const;

      virtual Data getBodyData() const;

      //virtual 
		  static const Mime& getStaticType() ;

      virtual std::ostream& encodeParsed(std::ostream& str) const;
      virtual void parse(ParseBuffer& pb);

      //Data& text() {checkParsed(); return mText;}
      
   private:
      static ContentsFactory<Pkcs7Contents> Factory;

      Data mText;
};


class Pkcs7SignedContents : public Pkcs7Contents
{
   public:
      Pkcs7SignedContents();
      Pkcs7SignedContents(const Data& text);
      Pkcs7SignedContents(HeaderFieldValue* hfv, const Mime& contentType);
      Pkcs7SignedContents(const Data& data, const Mime& contentType);
      Pkcs7SignedContents(const Pkcs7SignedContents& rhs);

      virtual ~Pkcs7SignedContents();

      Pkcs7SignedContents& operator=(const Pkcs7SignedContents& rhs);

      //virtual 
      static const Mime& getStaticType() ;
      virtual Contents* clone() const;

   private:
      static ContentsFactory<Pkcs7SignedContents> Factory;
};


}

#endif

/* ====================================================================
 * The Vovida Software License, Version 1.0 
 */