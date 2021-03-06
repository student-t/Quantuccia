/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer

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

/*! \file overnightindexedcoupon.hpp
    \brief coupon paying the compounded daily overnight rate
*/

#ifndef quantlib_overnight_indexed_coupon_hpp
#define quantlib_overnight_indexed_coupon_hpp

#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/indexes/iborindex.hpp>
#include <ql/time/schedule.hpp>

namespace QuantLib {

    //! overnight coupon
    /*! %Coupon paying the compounded interest due to daily overnight fixings.

        \warning telescopicValueDates optimizes the schedule for calculation speed,
        but might fail to produce correct results if the coupon ages by more than
        a grace period of 7 days. It is therefore recommended not to set this flag
        to true unless you know exactly what you are doing. The intended use is
        rather by the OISRateHelper which is safe, since it reinitialises the
        instrument each time the evaluation date changes.
    */
    class OvernightIndexedCoupon : public FloatingRateCoupon {
      public:
        OvernightIndexedCoupon(
                    const Date& paymentDate,
                    Real nominal,
                    const Date& startDate,
                    const Date& endDate,
                    const boost::shared_ptr<OvernightIndex>& overnightIndex,
                    Real gearing = 1.0,
                    Spread spread = 0.0,
                    const Date& refPeriodStart = Date(),
                    const Date& refPeriodEnd = Date(),
                    const DayCounter& dayCounter = DayCounter(),
                    bool telescopicValueDates = false);
        //! \name Inspectors
        //@{
        //! fixing dates for the rates to be compounded
        const std::vector<Date>& fixingDates() const { return fixingDates_; }
        //! accrual (compounding) periods
        const std::vector<Time>& dt() const { return dt_; }
        //! fixings to be compounded
        const std::vector<Rate>& indexFixings() const;
        //! value dates for the rates to be compounded
        const std::vector<Date>& valueDates() const { return valueDates_; }
        //@}
        //! \name FloatingRateCoupon interface
        //@{
        //! the date when the coupon is fully determined
        Date fixingDate() const { return fixingDates_.back(); }
        //@}
        //! \name Visitability
        //@{
        void accept(AcyclicVisitor&);
        //@}
      private:
        std::vector<Date> valueDates_, fixingDates_;
        mutable std::vector<Rate> fixings_;
        Size n_;
        std::vector<Time> dt_;
    };


    //! helper class building a sequence of overnight coupons
    class OvernightLeg {
      public:
        OvernightLeg(const Schedule& schedule,
                     const boost::shared_ptr<OvernightIndex>& overnightIndex);
        OvernightLeg& withNotionals(Real notional);
        OvernightLeg& withNotionals(const std::vector<Real>& notionals);
        OvernightLeg& withPaymentDayCounter(const DayCounter&);
        OvernightLeg& withPaymentAdjustment(BusinessDayConvention);
        OvernightLeg& withPaymentCalendar(const Calendar&);
        OvernightLeg& withPaymentLag(Natural lag);
        OvernightLeg& withGearings(Real gearing);
        OvernightLeg& withGearings(const std::vector<Real>& gearings);
        OvernightLeg& withSpreads(Spread spread);
        OvernightLeg& withSpreads(const std::vector<Spread>& spreads);
        OvernightLeg& withTelescopicValueDates(bool telescopicValueDates);
        operator Leg() const;
      private:
        Schedule schedule_;
        boost::shared_ptr<OvernightIndex> overnightIndex_;
        std::vector<Real> notionals_;
        DayCounter paymentDayCounter_;
        Calendar paymentCalendar_;
        BusinessDayConvention paymentAdjustment_;
        Natural paymentLag_;
        std::vector<Real> gearings_;
        std::vector<Spread> spreads_;
        bool telescopicValueDates_;
    };

}

/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2009 Roland Lichters
 Copyright (C) 2009 Ferdinando Ametrano
 Copyright (C) 2014 Peter Caspers
 Copyright (C) 2017 Joseph Jeisman
 Copyright (C) 2017 Fabrice Lecuyer

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

#include <ql/cashflows/couponpricer.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/utilities/vectors.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using std::vector;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

namespace QuantLib {

    namespace {

        class OvernightIndexedCouponPricer : public FloatingRateCouponPricer {
          public:
            void initialize(const FloatingRateCoupon& coupon) {
                coupon_ = dynamic_cast<const OvernightIndexedCoupon*>(&coupon);
                QL_ENSURE(coupon_, "wrong coupon type");
            }
            Rate swapletRate() const {

                shared_ptr<OvernightIndex> index =
                    dynamic_pointer_cast<OvernightIndex>(coupon_->index());

                const vector<Date>& fixingDates = coupon_->fixingDates();
                const vector<Time>& dt = coupon_->dt();

                Size n = dt.size(),
                     i = 0;

                Real compoundFactor = 1.0;

                // already fixed part
                Date today = Settings::instance().evaluationDate();
                while (i<n && fixingDates[i]<today) {
                    // rate must have been fixed
                    Rate pastFixing = IndexManager::instance().getHistory(
                                                index->name())[fixingDates[i]];
                    QL_REQUIRE(pastFixing != Null<Real>(),
                               "Missing " << index->name() <<
                               " fixing for " << fixingDates[i]);
                    compoundFactor *= (1.0 + pastFixing*dt[i]);
                    ++i;
                }

                // today is a border case
                if (i<n && fixingDates[i] == today) {
                    // might have been fixed
                    try {
                        Rate pastFixing = IndexManager::instance().getHistory(
                                                index->name())[fixingDates[i]];
                        if (pastFixing != Null<Real>()) {
                            compoundFactor *= (1.0 + pastFixing*dt[i]);
                            ++i;
                        } else {
                            ;   // fall through and forecast
                        }
                    } catch (Error&) {
                        ;       // fall through and forecast
                    }
                }

                // forward part using telescopic property in order
                // to avoid the evaluation of multiple forward fixings
                if (i<n) {
                    Handle<YieldTermStructure> curve =
                        index->forwardingTermStructure();
                    QL_REQUIRE(!curve.empty(),
                               "null term structure set to this instance of "<<
                               index->name());

                    const vector<Date>& dates = coupon_->valueDates();
                    DiscountFactor startDiscount = curve->discount(dates[i]);
                    DiscountFactor endDiscount = curve->discount(dates[n]);

                    compoundFactor *= startDiscount/endDiscount;
                }

                Rate rate = (compoundFactor - 1.0) / coupon_->accrualPeriod();
                return coupon_->gearing() * rate + coupon_->spread();
            }

            Real swapletPrice() const { QL_FAIL("swapletPrice not available");  }
            Real capletPrice(Rate) const { QL_FAIL("capletPrice not available"); }
            Rate capletRate(Rate) const { QL_FAIL("capletRate not available"); }
            Real floorletPrice(Rate) const { QL_FAIL("floorletPrice not available"); }
            Rate floorletRate(Rate) const { QL_FAIL("floorletRate not available"); }
          protected:
            const OvernightIndexedCoupon* coupon_;
        };
    }

  inline OvernightIndexedCoupon::OvernightIndexedCoupon(
                    const Date& paymentDate,
                    Real nominal,
                    const Date& startDate,
                    const Date& endDate,
                    const shared_ptr<OvernightIndex>& overnightIndex,
                    Real gearing,
                    Spread spread,
                    const Date& refPeriodStart,
                    const Date& refPeriodEnd,
                    const DayCounter& dayCounter,
                    bool telescopicValueDates)
    : FloatingRateCoupon(paymentDate, nominal, startDate, endDate,
                         overnightIndex->fixingDays(), overnightIndex,
                         gearing, spread,
                         refPeriodStart, refPeriodEnd,
                         dayCounter, false) {

        // value dates
        Date tmpEndDate = endDate;

        /* For the coupon's valuation only the first and last future valuation
           dates matter, therefore we can avoid to construct the whole series
           of valuation dates, a front and back stub will do. However notice
           that if the global evaluation date moves forward it might run past
           the front stub of valuation dates we build here (which incorporates
           a grace period of 7 business after the evluation date). This will
           lead to false coupon projections (see the warning the class header). */

        if (telescopicValueDates) {
            // build optimised value dates schedule: front stub goes
            // from start date to max(evalDate,startDate) + 7bd
            Date evalDate = Settings::instance().evaluationDate();
            tmpEndDate = overnightIndex->fixingCalendar().advance(
                std::max(startDate, evalDate), 7, Days, Following);
            tmpEndDate = std::min(tmpEndDate, endDate);
        }
        Schedule sch =
            MakeSchedule()
                .from(startDate)
                // .to(endDate)
                .to(tmpEndDate)
                .withTenor(1 * Days)
                .withCalendar(overnightIndex->fixingCalendar())
                .withConvention(overnightIndex->businessDayConvention())
                .backwards();
        valueDates_ = sch.dates();

        if (telescopicValueDates) {
            // build optimised value dates schedule: back stub
            // contains at least two dates
            Date tmp = overnightIndex->fixingCalendar().advance(
                endDate, -1, Days, Preceding);
            if (tmp != valueDates_.back())
                valueDates_.push_back(tmp);
            tmp = overnightIndex->fixingCalendar().adjust(
                endDate, overnightIndex->businessDayConvention());
            if (tmp != valueDates_.back())
                valueDates_.push_back(tmp);
        }

        QL_ENSURE(valueDates_.size()>=2, "degenerate schedule");

        // fixing dates
        n_ = valueDates_.size()-1;
        if (overnightIndex->fixingDays()==0) {
            fixingDates_ = vector<Date>(valueDates_.begin(),
                                             valueDates_.end()-1);
        } else {
            fixingDates_.resize(n_);
            for (Size i=0; i<n_; ++i)
                fixingDates_[i] = overnightIndex->fixingDate(valueDates_[i]);
        }

        // accrual (compounding) periods
        dt_.resize(n_);
        const DayCounter& dc = overnightIndex->dayCounter();
        for (Size i=0; i<n_; ++i)
            dt_[i] = dc.yearFraction(valueDates_[i], valueDates_[i+1]);

        setPricer(shared_ptr<FloatingRateCouponPricer>(new
                                            OvernightIndexedCouponPricer));
    }

  inline const vector<Rate>& OvernightIndexedCoupon::indexFixings() const {
        fixings_.resize(n_);
        for (Size i=0; i<n_; ++i)
            fixings_[i] = index_->fixing(fixingDates_[i]);
        return fixings_;
    }

  inline void OvernightIndexedCoupon::accept(AcyclicVisitor& v) {
        Visitor<OvernightIndexedCoupon>* v1 =
            dynamic_cast<Visitor<OvernightIndexedCoupon>*>(&v);
        if (v1 != 0) {
            v1->visit(*this);
        } else {
            FloatingRateCoupon::accept(v);
        }
    }

  inline OvernightLeg::OvernightLeg(const Schedule& schedule,
                               const shared_ptr<OvernightIndex>& i)
    : schedule_(schedule), overnightIndex_(i), paymentCalendar_(schedule.calendar()),
      paymentAdjustment_(Following), paymentLag_(0), telescopicValueDates_(false) {}

  inline OvernightLeg& OvernightLeg::withNotionals(Real notional) {
        notionals_ = vector<Real>(1, notional);
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withNotionals(const vector<Real>& notionals) {
        notionals_ = notionals;
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withPaymentDayCounter(const DayCounter& dc) {
        paymentDayCounter_ = dc;
        return *this;
    }

    inline OvernightLeg&
    OvernightLeg::withPaymentAdjustment(BusinessDayConvention convention) {
        paymentAdjustment_ = convention;
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withPaymentCalendar(const Calendar& cal) {
        paymentCalendar_ = cal;
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withPaymentLag(Natural lag) {
        paymentLag_ = lag;
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withGearings(Real gearing) {
        gearings_ = vector<Real>(1,gearing);
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withGearings(const vector<Real>& gearings) {
        gearings_ = gearings;
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withSpreads(Spread spread) {
        spreads_ = vector<Spread>(1,spread);
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withSpreads(const vector<Spread>& spreads) {
        spreads_ = spreads;
        return *this;
    }

  inline OvernightLeg& OvernightLeg::withTelescopicValueDates(bool telescopicValueDates) {
        telescopicValueDates_ = telescopicValueDates;
        return *this;
    }

  inline OvernightLeg::operator Leg() const {

        QL_REQUIRE(!notionals_.empty(), "no notional given");

        Leg cashflows;

        // the following is not always correct
        Calendar calendar = schedule_.calendar();

        Date refStart, start, refEnd, end;
        Date paymentDate;

        Size n = schedule_.size()-1;
        for (Size i=0; i<n; ++i) {
            refStart = start = schedule_.date(i);
            refEnd   =   end = schedule_.date(i+1);
            paymentDate = paymentCalendar_.advance(end, paymentLag_, Days, paymentAdjustment_);

            if (i == 0 && !schedule_.isRegular(i+1))
                refStart = calendar.adjust(end - schedule_.tenor(),
                                           paymentAdjustment_);
            if (i == n-1 && !schedule_.isRegular(i+1))
                refEnd = calendar.adjust(start + schedule_.tenor(),
                                         paymentAdjustment_);

            cashflows.push_back(shared_ptr<CashFlow>(new
                OvernightIndexedCoupon(paymentDate,
                                       detail::get(notionals_, i,
                                                   notionals_.back()),
                                       start, end,
                                       overnightIndex_,
                                       detail::get(gearings_, i, 1.0),
                                       detail::get(spreads_, i, 0.0),
                                       refStart, refEnd,
                                       paymentDayCounter_,
                                       telescopicValueDates_)));
        }
        return cashflows;
    }

}


#endif
