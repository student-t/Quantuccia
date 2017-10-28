/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Giorgio Facchinetti
 Copyright (C) 2007 Cristina Duminuco
 Copyright (C) 2011 Ferdinando Ametrano
 Copyright (C) 2015 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file couponpricer.hpp
    \brief Coupon pricers
*/

#ifndef quantlib_coupon_pricer_hpp
#define quantlib_coupon_pricer_hpp

#include <ql/termstructures/volatility/optionlet/optionletvolatilitystructure.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/cashflow.hpp>
#include <ql/option.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantLib {

    class FloatingRateCoupon;
    class IborCoupon;

    //! generic pricer for floating-rate coupons
    class FloatingRateCouponPricer: public virtual Observer,
                                    public virtual Observable {
      public:
        virtual ~FloatingRateCouponPricer() {}
        //! \name required interface
        //@{
        virtual Real swapletPrice() const = 0;
        virtual Rate swapletRate() const = 0;
        virtual Real capletPrice(Rate effectiveCap) const = 0;
        virtual Rate capletRate(Rate effectiveCap) const = 0;
        virtual Real floorletPrice(Rate effectiveFloor) const = 0;
        virtual Rate floorletRate(Rate effectiveFloor) const = 0;
        virtual void initialize(const FloatingRateCoupon& coupon) = 0;
        //@}
        //! \name Observer interface
        //@{
        void update(){notifyObservers();}
        //@}
    };

    //! base pricer for capped/floored Ibor coupons
    class IborCouponPricer : public FloatingRateCouponPricer {
      public:
        IborCouponPricer(const Handle<OptionletVolatilityStructure>& v =
                                          Handle<OptionletVolatilityStructure>())
        : capletVol_(v) { registerWith(capletVol_); }

        Handle<OptionletVolatilityStructure> capletVolatility() const{
            return capletVol_;
        }
        void setCapletVolatility(
                            const Handle<OptionletVolatilityStructure>& v =
                                    Handle<OptionletVolatilityStructure>()) {
            unregisterWith(capletVol_);
            capletVol_ = v;
            registerWith(capletVol_);
            update();
        }
      private:
        Handle<OptionletVolatilityStructure> capletVol_;
    };

    /*! Black-formula pricer for capped/floored Ibor coupons
        References for timing adjustments
        Black76             Hull, Options, Futures and other
                            derivatives, 4th ed., page 550
        BivariateLognormal  http://ssrn.com/abstract=2170721
        The bivariate lognormal adjustment implementation is
        still considered experimental */
    class BlackIborCouponPricer : public IborCouponPricer {
      public:
        enum TimingAdjustment { Black76, BivariateLognormal };
        BlackIborCouponPricer(
            const Handle< OptionletVolatilityStructure > &v =
                Handle< OptionletVolatilityStructure >(),
            const TimingAdjustment timingAdjustment = Black76,
            const Handle< Quote > correlation =
                Handle< Quote >(boost::shared_ptr<Quote>(
                                                   new SimpleQuote(1.0))))
            : IborCouponPricer(v), timingAdjustment_(timingAdjustment),
              correlation_(correlation) {
            QL_REQUIRE(timingAdjustment_ == Black76 ||
                           timingAdjustment_ == BivariateLognormal,
                       "unknown timing adjustment (code " << timingAdjustment_
                                                          << ")");
            registerWith(correlation_);
        };
        virtual void initialize(const FloatingRateCoupon& coupon);
        Real swapletPrice() const;
        Rate swapletRate() const;
        Real capletPrice(Rate effectiveCap) const;
        Rate capletRate(Rate effectiveCap) const;
        Real floorletPrice(Rate effectiveFloor) const;
        Rate floorletRate(Rate effectiveFloor) const;

      protected:
        Real optionletPrice(Option::Type optionType,
                            Real effStrike) const;

        virtual Rate adjustedFixing(Rate fixing = Null<Rate>()) const;

        Real gearing_;
        Spread spread_;
        Time accrualPeriod_;
        boost::shared_ptr<IborIndex> index_;
        Real discount_;
        Real spreadLegValue_;

        const FloatingRateCoupon* coupon_;

      private:
        const TimingAdjustment timingAdjustment_;
        const Handle< Quote > correlation_;
    };

    //! base pricer for vanilla CMS coupons
    class CmsCouponPricer : public FloatingRateCouponPricer {
      public:
        CmsCouponPricer(const Handle<SwaptionVolatilityStructure>& v =
                                        Handle<SwaptionVolatilityStructure>())
        : swaptionVol_(v) { registerWith(swaptionVol_); }

        Handle<SwaptionVolatilityStructure> swaptionVolatility() const{
            return swaptionVol_;
        }
        void setSwaptionVolatility(
                            const Handle<SwaptionVolatilityStructure>& v=
                                    Handle<SwaptionVolatilityStructure>()) {
            unregisterWith(swaptionVol_);
            swaptionVol_ = v;
            registerWith(swaptionVol_);
            update();
        }
      private:
        Handle<SwaptionVolatilityStructure> swaptionVol_;
    };

    /*! (CMS) coupon pricer that has a mean reversion parameter which can be
      used to calibrate to cms market quotes */
    class MeanRevertingPricer {
    public:
        virtual Real meanReversion() const = 0;
        virtual void setMeanReversion(const Handle<Quote>&) = 0;
        virtual ~MeanRevertingPricer() {}
    };

    void setCouponPricer(const Leg& leg,
                         const boost::shared_ptr<FloatingRateCouponPricer>&);

    void setCouponPricers(
            const Leg& leg,
            const std::vector<boost::shared_ptr<FloatingRateCouponPricer> >&);

    /*! set the first matching pricer (if any) to each coupon of the leg */
    void setCouponPricers(
            const Leg& leg,
            const boost::shared_ptr<FloatingRateCouponPricer>&,
            const boost::shared_ptr<FloatingRateCouponPricer>&);

    void setCouponPricers(
            const Leg& leg,
            const boost::shared_ptr<FloatingRateCouponPricer>&,
            const boost::shared_ptr<FloatingRateCouponPricer>&,
            const boost::shared_ptr<FloatingRateCouponPricer>&);

    void setCouponPricers(
            const Leg& leg,
            const boost::shared_ptr<FloatingRateCouponPricer>&,
            const boost::shared_ptr<FloatingRateCouponPricer>&,
            const boost::shared_ptr<FloatingRateCouponPricer>&,
            const boost::shared_ptr<FloatingRateCouponPricer>&);

    // inline

    inline inline Real BlackIborCouponPricer::swapletPrice() const {
        // past or future fixing is managed in InterestRateIndex::fixing()

        Real swapletPrice = adjustedFixing() * accrualPeriod_ * discount_;
        return gearing_ * swapletPrice + spreadLegValue_;
    }

    inline inline Rate BlackIborCouponPricer::swapletRate() const {
        return swapletPrice()/(accrualPeriod_*discount_);
    }

    inline inline Real BlackIborCouponPricer::capletPrice(Rate effectiveCap) const {
        Real capletPrice = optionletPrice(Option::Call, effectiveCap);
        return gearing_ * capletPrice;
    }

    inline inline Rate BlackIborCouponPricer::capletRate(Rate effectiveCap) const {
        return capletPrice(effectiveCap) / (accrualPeriod_*discount_);
    }

    inline
    inline Real BlackIborCouponPricer::floorletPrice(Rate effectiveFloor) const {
        Real floorletPrice = optionletPrice(Option::Put, effectiveFloor);
        return gearing_ * floorletPrice;
    }

    inline
    inline Rate BlackIborCouponPricer::floorletRate(Rate effectiveFloor) const {
        return floorletPrice(effectiveFloor) / (accrualPeriod_*discount_);
    }

}
/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2007 Giorgio Facchinetti
 Copyright (C) 2007 Cristina Duminuco
 Copyright (C) 2011 Ferdinando Ametrano
 Copyright (C) 2015 Peter Caspers

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/digitalcoupon.hpp>
#include <ql/cashflows/digitalcmscoupon.hpp>
#include <ql/cashflows/digitaliborcoupon.hpp>
#include <ql/cashflows/rangeaccrual.hpp>
#include <ql/experimental/coupons/subperiodcoupons.hpp> /* internal */
#include <ql/experimental/coupons/cmsspreadcoupon.hpp>  /* internal */
#include <ql/experimental/coupons/digitalcmsspreadcoupon.hpp>  /* internal */
#include <ql/pricingengines/blackformula.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using boost::dynamic_pointer_cast;

namespace QuantLib {

//===========================================================================//
//                              BlackIborCouponPricer                        //
//===========================================================================//

    inline void BlackIborCouponPricer::initialize(const FloatingRateCoupon& coupon) {

        gearing_ = coupon.gearing();
        spread_ = coupon.spread();
        accrualPeriod_ = coupon.accrualPeriod();
        QL_REQUIRE(accrualPeriod_ != 0.0, "null accrual period");

        index_ = dynamic_pointer_cast<IborIndex>(coupon.index());
        if (!index_) {
            // check if the coupon was right
            const IborCoupon* c = dynamic_cast<const IborCoupon*>(&coupon);
            QL_REQUIRE(c, "IborCoupon required");
            // coupon was right, index is not
            QL_FAIL("IborIndex required");
        }
        Handle<YieldTermStructure> rateCurve =
                                            index_->forwardingTermStructure();

        Date paymentDate = coupon.date();
        if (paymentDate > rateCurve->referenceDate())
            discount_ = rateCurve->discount(paymentDate);
        else
            discount_ = 1.0;

        spreadLegValue_ = spread_ * accrualPeriod_ * discount_;

        coupon_ = &coupon;
    }

    inline Real BlackIborCouponPricer::optionletPrice(Option::Type optionType,
                                               Real effStrike) const {
        Date fixingDate = coupon_->fixingDate();
        if (fixingDate <= Settings::instance().evaluationDate()) {
            // the amount is determined
            Real a, b;
            if (optionType==Option::Call) {
                a = coupon_->indexFixing();
                b = effStrike;
            } else {
                a = effStrike;
                b = coupon_->indexFixing();
            }
            return std::max(a - b, 0.0)* accrualPeriod_*discount_;
        } else {
            // not yet determined, use Black model
            QL_REQUIRE(!capletVolatility().empty(),
                       "missing optionlet volatility");
            Real stdDev =
                std::sqrt(capletVolatility()->blackVariance(fixingDate,
                                                            effStrike));
            Real shift = capletVolatility()->displacement();
            bool shiftedLn =
                capletVolatility()->volatilityType() == ShiftedLognormal;
            Rate fixing =
                shiftedLn
                    ? blackFormula(optionType, effStrike, adjustedFixing(),
                                   stdDev, 1.0, shift)
                    : bachelierBlackFormula(optionType, effStrike,
                                            adjustedFixing(), stdDev, 1.0);
            return fixing * accrualPeriod_ * discount_;
        }
    }

    inline Rate BlackIborCouponPricer::adjustedFixing(Rate fixing) const {

        if (fixing == Null<Rate>())
            fixing = coupon_->indexFixing();

        if (!coupon_->isInArrears() && timingAdjustment_ == Black76)
            return fixing;

        QL_REQUIRE(!capletVolatility().empty(),
                   "missing optionlet volatility");
        Date d1 = coupon_->fixingDate();
        Date referenceDate = capletVolatility()->referenceDate();
        if (d1 <= referenceDate)
            return fixing;
        Date d2 = index_->valueDate(d1);
        Date d3 = index_->maturityDate(d2);
        Time tau = index_->dayCounter().yearFraction(d2, d3);
        Real variance = capletVolatility()->blackVariance(d1, fixing);

        Real shift = capletVolatility()->displacement();
        bool shiftedLn =
            capletVolatility()->volatilityType() == ShiftedLognormal;

        Spread adjustment = shiftedLn
                                ? (fixing + shift) * (fixing + shift) *
                                      variance * tau / (1.0 + fixing * tau)
                                : variance * tau / (1.0 + fixing * tau);

        if (timingAdjustment_ == BivariateLognormal) {
            QL_REQUIRE(!correlation_.empty(), "no correlation given");
            Date d4 = coupon_->date();
            Date d5 = d4 >= d3 ? d3 : d2;
            Time tau2 = index_->dayCounter().yearFraction(d5, d4);
            if (d4 >= d3)
                adjustment = 0.0;
            // if d4 < d2 (payment before index start) we just apply the
            // Black76 in arrears adjustment
            if (tau2 > 0.0) {
                Real fixing2 =
                    (index_->forwardingTermStructure()->discount(d5) /
                         index_->forwardingTermStructure()->discount(d4) -
                     1.0) /
                    tau2;
                adjustment -= shiftedLn
                                  ? correlation_->value() * tau2 * variance *
                                        (fixing + shift) * (fixing2 + shift) /
                                        (1.0 + fixing2 * tau2)
                                  : correlation_->value() * tau2 * variance /
                                        (1.0 + fixing2 * tau2);
            }
        }
        return fixing + adjustment;
    }

//===========================================================================//
//                         CouponSelectorToSetPricer                         //
//===========================================================================//

    namespace {

        class PricerSetter : public AcyclicVisitor,
                             public Visitor<CashFlow>,
                             public Visitor<Coupon>,
                             public Visitor<FloatingRateCoupon>,
                             public Visitor<CappedFlooredCoupon>,
                             public Visitor<IborCoupon>,
                             public Visitor<CmsCoupon>,
                             public Visitor<CmsSpreadCoupon>,
                             public Visitor<CappedFlooredIborCoupon>,
                             public Visitor<CappedFlooredCmsCoupon>,
                             public Visitor<CappedFlooredCmsSpreadCoupon>,
                             public Visitor<DigitalIborCoupon>,
                             public Visitor<DigitalCmsCoupon>,
                             public Visitor<DigitalCmsSpreadCoupon>,
                             public Visitor<RangeAccrualFloatersCoupon>,
                             public Visitor<SubPeriodsCoupon> {
          private:
            boost::shared_ptr<FloatingRateCouponPricer> pricer_;
          public:
            explicit PricerSetter(
                    const boost::shared_ptr<FloatingRateCouponPricer>& pricer)
            : pricer_(pricer) {}

            void visit(CashFlow& c);
            void visit(Coupon& c);
            void visit(FloatingRateCoupon& c);
            void visit(CappedFlooredCoupon& c);
            void visit(IborCoupon& c);
            void visit(CappedFlooredIborCoupon& c);
            void visit(DigitalIborCoupon& c);
            void visit(CmsCoupon& c);
            void visit(CmsSpreadCoupon& c);
            void visit(CappedFlooredCmsCoupon& c);
            void visit(CappedFlooredCmsSpreadCoupon& c);
            void visit(DigitalCmsCoupon& c);
            void visit(DigitalCmsSpreadCoupon& c);
            void visit(RangeAccrualFloatersCoupon& c);
            void visit(SubPeriodsCoupon& c);
        };

        inline void PricerSetter::visit(CashFlow&) {
            // nothing to do
        }

        inline void PricerSetter::visit(Coupon&) {
            // nothing to do
        }

        inline void PricerSetter::visit(FloatingRateCoupon& c) {
            c.setPricer(pricer_);
        }

        inline void PricerSetter::visit(CappedFlooredCoupon& c) {
            // we might end up here because a CappedFlooredCoupon
            // was directly constructed; we should then check
            // the underlying for consistency with the pricer
            if (boost::dynamic_pointer_cast<IborCoupon>(c.underlying())) {
                QL_REQUIRE(boost::dynamic_pointer_cast<IborCouponPricer>(pricer_),
                           "pricer not compatible with Ibor Coupon");
            } else if (boost::dynamic_pointer_cast<CmsCoupon>(c.underlying())) {
                QL_REQUIRE(boost::dynamic_pointer_cast<CmsCouponPricer>(pricer_),
                           "pricer not compatible with CMS Coupon");
            } else if (boost::dynamic_pointer_cast<CmsSpreadCoupon>(c.underlying())) {
                QL_REQUIRE(boost::dynamic_pointer_cast<CmsSpreadCouponPricer>(pricer_),
                           "pricer not compatible with CMS spread Coupon");
            }
            c.setPricer(pricer_);
        }

        inline void PricerSetter::visit(IborCoupon& c) {
            const boost::shared_ptr<IborCouponPricer> iborCouponPricer =
                boost::dynamic_pointer_cast<IborCouponPricer>(pricer_);
            QL_REQUIRE(iborCouponPricer,
                       "pricer not compatible with Ibor coupon");
            c.setPricer(iborCouponPricer);
        }

        inline void PricerSetter::visit(DigitalIborCoupon& c) {
            const boost::shared_ptr<IborCouponPricer> iborCouponPricer =
                boost::dynamic_pointer_cast<IborCouponPricer>(pricer_);
            QL_REQUIRE(iborCouponPricer,
                       "pricer not compatible with Ibor coupon");
            c.setPricer(iborCouponPricer);
        }

        inline void PricerSetter::visit(CappedFlooredIborCoupon& c) {
            const boost::shared_ptr<IborCouponPricer> iborCouponPricer =
                boost::dynamic_pointer_cast<IborCouponPricer>(pricer_);
            QL_REQUIRE(iborCouponPricer,
                       "pricer not compatible with Ibor coupon");
            c.setPricer(iborCouponPricer);
        }

        inline void PricerSetter::visit(CmsCoupon& c) {
            const boost::shared_ptr<CmsCouponPricer> cmsCouponPricer =
                boost::dynamic_pointer_cast<CmsCouponPricer>(pricer_);
            QL_REQUIRE(cmsCouponPricer,
                       "pricer not compatible with CMS coupon");
            c.setPricer(cmsCouponPricer);
        }

        inline void PricerSetter::visit(CmsSpreadCoupon& c) {
            const boost::shared_ptr<CmsSpreadCouponPricer> cmsSpreadCouponPricer =
                boost::dynamic_pointer_cast<CmsSpreadCouponPricer>(pricer_);
            QL_REQUIRE(cmsSpreadCouponPricer,
                       "pricer not compatible with CMS spread coupon");
            c.setPricer(cmsSpreadCouponPricer);
        }

        inline void PricerSetter::visit(CappedFlooredCmsCoupon& c) {
            const boost::shared_ptr<CmsCouponPricer> cmsCouponPricer =
                boost::dynamic_pointer_cast<CmsCouponPricer>(pricer_);
            QL_REQUIRE(cmsCouponPricer,
                       "pricer not compatible with CMS coupon");
            c.setPricer(cmsCouponPricer);
        }

        inline void PricerSetter::visit(CappedFlooredCmsSpreadCoupon& c) {
            const boost::shared_ptr<CmsSpreadCouponPricer> cmsSpreadCouponPricer =
                boost::dynamic_pointer_cast<CmsSpreadCouponPricer>(pricer_);
            QL_REQUIRE(cmsSpreadCouponPricer,
                       "pricer not compatible with CMS spread coupon");
            c.setPricer(cmsSpreadCouponPricer);
        }

        inline void PricerSetter::visit(DigitalCmsCoupon& c) {
            const boost::shared_ptr<CmsCouponPricer> cmsCouponPricer =
                boost::dynamic_pointer_cast<CmsCouponPricer>(pricer_);
            QL_REQUIRE(cmsCouponPricer,
                       "pricer not compatible with CMS coupon");
            c.setPricer(cmsCouponPricer);
        }

        inline void PricerSetter::visit(DigitalCmsSpreadCoupon& c) {
            const boost::shared_ptr<CmsSpreadCouponPricer> cmsSpreadCouponPricer =
                boost::dynamic_pointer_cast<CmsSpreadCouponPricer>(pricer_);
            QL_REQUIRE(cmsSpreadCouponPricer,
                       "pricer not compatible with CMS spread coupon");
            c.setPricer(cmsSpreadCouponPricer);
        }

        inline void PricerSetter::visit(RangeAccrualFloatersCoupon& c) {
            const boost::shared_ptr<RangeAccrualPricer> rangeAccrualPricer =
                boost::dynamic_pointer_cast<RangeAccrualPricer>(pricer_);
            QL_REQUIRE(rangeAccrualPricer,
                       "pricer not compatible with range-accrual coupon");
            c.setPricer(rangeAccrualPricer);
        }

        inline void PricerSetter::visit(SubPeriodsCoupon& c) {
            const boost::shared_ptr<SubPeriodsPricer> subPeriodsPricer =
                boost::dynamic_pointer_cast<SubPeriodsPricer>(pricer_);
            QL_REQUIRE(subPeriodsPricer,
                       "pricer not compatible with sub-period coupon");
            c.setPricer(subPeriodsPricer);
        }

        inline void setCouponPricersFirstMatching(const Leg& leg,
                                           const std::vector<boost::shared_ptr<FloatingRateCouponPricer> >& p) {
            std::vector<PricerSetter> setter;
            for (Size i = 0; i < p.size(); ++i) {
                setter.push_back(PricerSetter(p[i]));
            }
            for (Size i = 0; i < leg.size(); ++i) {
                Size j = 0;
                do {
                    try {
                        leg[i]->accept(setter[j]);
                        j = p.size();
                    } catch (...) {
                        ++j;
                    }
                } while (j < p.size());
            }
        }

    } // anonymous namespace

    inline void setCouponPricer(const Leg& leg, const boost::shared_ptr<FloatingRateCouponPricer>& pricer) {
            PricerSetter setter(pricer);
            for (Size i = 0; i < leg.size(); ++i) {
                leg[i]->accept(setter);
            }
    }

    inline void setCouponPricers(
            const Leg& leg,
            const std::vector<boost::shared_ptr<FloatingRateCouponPricer> >&
                                                                    pricers) {
        Size nCashFlows = leg.size();
        QL_REQUIRE(nCashFlows>0, "no cashflows");

        Size nPricers = pricers.size();
        QL_REQUIRE(nCashFlows >= nPricers,
                   "mismatch between leg size (" << nCashFlows <<
                   ") and number of pricers (" << nPricers << ")");

        for (Size i=0; i<nCashFlows; ++i) {
            PricerSetter setter(i<nPricers ? pricers[i] : pricers[nPricers-1]);
            leg[i]->accept(setter);
        }
    }

    inline void setCouponPricers(
            const Leg& leg,
            const boost::shared_ptr<FloatingRateCouponPricer>& p1,
            const boost::shared_ptr<FloatingRateCouponPricer>& p2) {
        std::vector<boost::shared_ptr<FloatingRateCouponPricer> > p;
        p.push_back(p1);
        p.push_back(p2);
        setCouponPricersFirstMatching(leg, p);
    }

    inline void setCouponPricers(
            const Leg& leg,
            const boost::shared_ptr<FloatingRateCouponPricer>& p1,
            const boost::shared_ptr<FloatingRateCouponPricer>& p2,
            const boost::shared_ptr<FloatingRateCouponPricer>& p3) {
        std::vector<boost::shared_ptr<FloatingRateCouponPricer> > p;
        p.push_back(p1);
        p.push_back(p2);
        p.push_back(p3);
        setCouponPricersFirstMatching(leg, p);
    }

    inline void setCouponPricers(
            const Leg& leg,
            const boost::shared_ptr<FloatingRateCouponPricer>& p1,
            const boost::shared_ptr<FloatingRateCouponPricer>& p2,
            const boost::shared_ptr<FloatingRateCouponPricer>& p3,
            const boost::shared_ptr<FloatingRateCouponPricer>& p4) {
        std::vector<boost::shared_ptr<FloatingRateCouponPricer> > p;
        p.push_back(p1);
        p.push_back(p2);
        p.push_back(p3);
        p.push_back(p4);
        setCouponPricersFirstMatching(leg, p);
    }


}

#endif
