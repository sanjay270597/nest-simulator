/*
 *  stdp_pl_synapse_hom_ax_delay.h
 *
 *  This file is part of NEST.
 *
 *  Copyright (C) 2004 The NEST Initiative
 *
 *  NEST is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  NEST is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with NEST.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef STDP_PL_SYNAPSE_HOM_AX_DELAY_H
#define STDP_PL_SYNAPSE_HOM_AX_DELAY_H

// C++ includes:
#include <cmath>
#include <iomanip>

// Includes from nestkernel:
#include "connection.h"
#include "adjustentry.h"

namespace nest
{

/* BeginUserDocs: synapse, spike-timing-dependent plasticity

Short description
+++++++++++++++++

Synapse type for spike-timing dependent plasticity with power law

Description
+++++++++++

``stdp_pl_synapse`` is a connector to create synapses with spike time
dependent plasticity using homoegeneous parameters (as defined in [1]_).

Parameters
++++++++++

=========  ======  ====================================================
 tau_plus  ms      Time constant of STDP window, potentiation
                   (tau_minus defined in postsynaptic neuron)
 lambda    real    Learning rate
 alpha     real    Asymmetry parameter (scales depressing increments as
                   alpha*lambda)
 mu        real    Weight dependence exponent, potentiation
=========  ======  ====================================================

The parameters can only be set by SetDefaults and apply to all synapses of
the model.

.. warning::

   This synaptic plasticity rule does not take
   :ref:`precise spike timing <sim_precise_spike_times>` into
   account. When calculating the weight update, the precise spike time part
   of the timestamp is ignored.

References
++++++++++

.. [1] Morrison A, Aertsen A, Diesmann M. (2007) Spike-timing dependent
       plasticity in balanced random netrks. Neural Computation,
       19(6):1437-1467. DOI: https://doi.org/10.1162/neco.2007.19.6.1437

Transmits
+++++++++

SpikeEvent

See also
++++++++

stdp_synapse, tsodyks_synapse, static_synapse

EndUserDocs */

/**
 * Class containing the common properties for all synapses of type
 * stdp_pl_synapse_hom_ax_delay.
 */
class STDPPLHomAxDelayCommonProperties : public CommonSynapseProperties
{

public:
  /**
   * Default constructor.
   * Sets all property values to defaults.
   */
  STDPPLHomAxDelayCommonProperties();

  /**
   * Get all properties and put them into a dictionary.
   */
  void get_status( DictionaryDatum& d ) const;

  /**
   * Set properties from the values given in dictionary.
   */
  void set_status( const DictionaryDatum& d, ConnectorModel& cm );

  // data members common to all connections
  double tau_plus_;
  double tau_plus_inv_; //!< 1 / tau_plus for efficiency
  double lambda_;
  double alpha_;
  double mu_;
  double axonal_delay_; // Axonal Delay Parameter
};


/**
 * Class representing an STDP connection with homogeneous parameters, i.e.
 * parameters are the same for all synapses.
 */
template < typename targetidentifierT >
class stdp_pl_synapse_hom_ax_delay : public Connection< targetidentifierT >
{

public:
  typedef STDPPLHomAxDelayCommonProperties CommonPropertiesType;
  typedef Connection< targetidentifierT > ConnectionBase;


  /**
   * Default Constructor.
   * Sets default values for all parameters. Needed by GenericConnectorModel.
   */
  stdp_pl_synapse_hom_ax_delay();

  /**
   * Copy constructor from a property object.
   * Needs to be defined properly in order for GenericConnector to work.
   */
  stdp_pl_synapse_hom_ax_delay( const stdp_pl_synapse_hom_ax_delay& ) = default;

  // Explicitly declare all methods inherited from the dependent base
  // ConnectionBase. This avoids explicit name prefixes in all places these
  // functions are used. Since ConnectionBase depends on the template parameter,
  // they are not automatically found in the base class.
  using ConnectionBase::get_delay;
  using ConnectionBase::get_delay_steps;
  using ConnectionBase::get_rport;
  using ConnectionBase::get_target;

  /**
   * Get all properties of this connection and put them into a dictionary.
   */
  void get_status( DictionaryDatum& d ) const;

  /**
   * Set properties of this connection from the values given in dictionary.
   */
  void set_status( const DictionaryDatum& d, ConnectorModel& cm );

  /**
   * Send an event to the receiver of this connection.
   * \param e The event to send
   */
  void send( Event& e, thread t, const STDPPLHomAxDelayCommonProperties& );

  /**
   * Adjusts the weight according to the missed spike.
   */
  void adjust_weight( adjustentry* a, const double missing_spike, const STDPPLHomAxDelayCommonProperties& cp );

  class ConnTestDummyNode : public ConnTestDummyNodeBase
  {
  public:
    // Ensure proper overriding of overloaded virtual functions.
    // Return values from functions are ignored.
    using ConnTestDummyNodeBase::handles_test_event;
    port
    handles_test_event( SpikeEvent&, rport )
    {
      return invalid_port_;
    }
  };

  /*
   * This function calls check_connection on the sender and checks if the
   * receiver accepts the event type and receptor type requested by the sender.
   * Node::check_connection() will either confirm the receiver port by returning
   * true or false if the connection should be ignored.
   * We have to override the base class' implementation, since for STDP
   * connections we have to call register_stdp_pl_connection on the target
   * neuron to inform the Archiver to collect spikes for this connection.
   *
   * \param s The source node
   * \param r The target node
   * \param receptor_type The ID of the requested receptor type
   */
  void
  check_connection( Node& s, Node& t, rport receptor_type, const CommonPropertiesType& )
  {
    ConnTestDummyNode dummy_target;

    ConnectionBase::check_connection_( dummy_target, s, t, receptor_type );

    t.register_stdp_connection( t_lastspike_ - get_delay(), get_delay() );
  }

  void
  set_weight( double w )
  {
    weight_ = w;
  }

private:
  double
  facilitate_( double w, double kplus, const STDPPLHomAxDelayCommonProperties& cp )
  {
    return w + ( cp.lambda_ * std::pow( w, cp.mu_ ) * kplus );
  }

  double
  depress_( double w, double kminus, const STDPPLHomAxDelayCommonProperties& cp )
  {
    double new_w = w - ( cp.lambda_ * cp.alpha_ * w * kminus );
    return new_w > 0.0 ? new_w : 0.0;
  }

  // data members of each connection
  double weight_;
  double Kplus_;
  double t_lastspike_;
};

//
// Implementation of class stdp_pl_synapse_hom_ax_delay.
//

/**
 * Send an event to the receiver of this connection.
 * \param e The event to send
 * \param p The port under which this connection is stored in the Connector.
 */
template < typename targetidentifierT >
inline void
stdp_pl_synapse_hom_ax_delay< targetidentifierT >::send( Event& e, thread t, const STDPPLHomAxDelayCommonProperties& cp )
{
  // synapse STDP depressing/facilitation dynamics

  const double t_spike = e.get_stamp().get_ms();

  // t_lastspike_ = 0 initially

  Node* target = get_target( t );

  double dendritic_delay = get_delay() - cp.axonal_delay_;

  // get spike history in relevant range (t1, t2] from postsynaptic neuron
  std::deque< histentry >::iterator start;
  std::deque< histentry >::iterator finish;
  target->get_history( t_lastspike_ - dendritic_delay + cp.axonal_delay_, t_spike - dendritic_delay + cp.axonal_delay_, &start, &finish );

  std::cout << std::setprecision(15) << "get_history\t" << t_lastspike_ - dendritic_delay + cp.axonal_delay_ << ", " << t_spike - dendritic_delay + cp.axonal_delay_ << "\t" << ( start != finish) << std::endl;
  
  // facilitation due to postsynaptic spikes since last pre-synaptic spike
  double minus_dt;
  while ( start != finish )
  {
    minus_dt = t_lastspike_ + cp.axonal_delay_ - ( start->t_ + dendritic_delay );
    // get_history() should make sure that
    // start->t_ > t_lastspike - dendritic_delay, i.e. minus_dt < 0
    assert( minus_dt < -1.0 * kernel().connection_manager.get_stdp_eps() );
    std::cout << std::setprecision(15) << "facilitation\t" << weight_ << "\t";
    weight_ = facilitate_( weight_, Kplus_ * std::exp( minus_dt * cp.tau_plus_inv_ ), cp );
    std::cout << std::setprecision(15) << "\t" << t_lastspike_ + cp.axonal_delay_ << "\t" << start->t_ + dendritic_delay << "\t" << Kplus_ * std::exp( minus_dt * cp.tau_plus_inv_) << "\t" << weight_ << std::endl;
    start++;
  }

  // store weight before depression for potential adjustment
  const double old_weight = weight_;

  // depression due to new pre-synaptic spike
  const double K_minus = target->get_K_value( t_spike + cp.axonal_delay_ - dendritic_delay );
  std::cout << std::setprecision(15) << "depression\t" << weight_ << "\t";
  weight_ = depress_( weight_, K_minus, cp );
  std::cout << std::setprecision(15) << "\t" << t_lastspike_ + cp.axonal_delay_ - minus_dt << "\t" << t_spike + cp.axonal_delay_ << "\t" << K_minus << "\t" << weight_ << std::endl;

  e.set_receiver( *target );
  e.set_weight( weight_ );
  e.set_delay_steps( get_delay_steps() );
  e.set_rport( get_rport() );
  e();
  
  //std::cout << "axonal delay = " <<  cp.axonal_delay_ << " dendritic delay = " << dendritic_delay << std::endl;

  if ( cp.axonal_delay_ > dendritic_delay )
  {
    //std::cout << "axonal_delay_ > dendritic_delay\n";	
    SpikeData sender_spike_data_ = e.get_sender_spike_data();
    adjustentry a = adjustentry( t_lastspike_, old_weight,
				 t_spike + cp.axonal_delay_ - dendritic_delay,
				 sender_spike_data_.get_tid(),
				 sender_spike_data_.get_syn_id(),
				 sender_spike_data_.get_lcid() );
    target->add_synapse_to_check(a);
  }

  Kplus_ = Kplus_ * std::exp( ( t_lastspike_ - t_spike ) * cp.tau_plus_inv_ ) + 1.0;

  t_lastspike_ = t_spike;
}

template < typename targetidentifierT >
stdp_pl_synapse_hom_ax_delay< targetidentifierT >::stdp_pl_synapse_hom_ax_delay()
  : ConnectionBase()
  , weight_( 1.0 )
  , Kplus_( 0.0 )
  , t_lastspike_( 0.0 )
{
}

template < typename targetidentifierT >
void
stdp_pl_synapse_hom_ax_delay< targetidentifierT >::get_status( DictionaryDatum& d ) const
{

  // base class properties, different for individual synapse
  ConnectionBase::get_status( d );
  def< double >( d, names::weight, weight_ );

  // own properties, different for individual synapse
  def< double >( d, names::Kplus, Kplus_ );
  def< long >( d, names::size_of, sizeof( *this ) );
}

template < typename targetidentifierT >
void
stdp_pl_synapse_hom_ax_delay< targetidentifierT >::set_status( const DictionaryDatum& d, ConnectorModel& cm )
{
  // base class properties
  ConnectionBase::set_status( d, cm );
  updateValue< double >( d, names::weight, weight_ );

  updateValue< double >( d, names::Kplus, Kplus_ );
}

template < typename targetidentifierT >
inline
void stdp_pl_synapse_hom_ax_delay< targetidentifierT >::adjust_weight( adjustentry* a, 
  const double missing_spike, const STDPPLHomAxDelayCommonProperties& cp )
{
  //std::cout << "adjusting weights\n";
  const double ori_weight_ = weight_;
  weight_ = a->old_weight_; // removes the last depressive step
  
  Node* target = get_target( a->tid_ );

  double dendritic_delay = get_delay() - cp.axonal_delay_;
  double t_spike = a->t_received_ - cp.axonal_delay_ + dendritic_delay;

  std::deque<histentry>::iterator start;
  std::deque<histentry>::iterator finish;

  // we know the time but read it anyway as this then keeps the access counter correct
  target->get_history( missing_spike - 1e-3,
		       missing_spike + 1e-3,
		       &start, &finish);

  while ( start != finish )
  {
    std::cout << std::setprecision(15) << "found missing spike in hist: " << start->t_ << std::endl;
    start++;
  }

  // facilitation due to new post-synaptic spike
  const double minus_dt = a->t_lastspike_ + cp.axonal_delay_ - ( missing_spike + dendritic_delay );
  assert( minus_dt < -1.0 * kernel().connection_manager.get_stdp_eps() );
  double Kplus_corr = ( Kplus_- 1.0 ) / std::exp( ( a->t_lastspike_ - t_spike ) / cp.tau_plus_ ); //TODO: check with test
  std::cout << std::setprecision(15) << "adj facilitation\t" << weight_ << "\t";
  weight_ = facilitate_(weight_, Kplus_corr * std::exp( minus_dt / cp.tau_plus_ ), cp );
  std::cout << std::setprecision(15) << "\t" << a->t_lastspike_ + cp.axonal_delay_ << "\t" << missing_spike + dendritic_delay << "\t" << Kplus_corr * std::exp( minus_dt * cp.tau_plus_inv_) << "\t" << weight_ << std::endl;

  // update adjustentry in case there are more post spikes
  a->old_weight_ = weight_;

  // depression taking into account new post-synaptic spike
  const double K_minus = target->get_K_value( t_spike + cp.axonal_delay_ - dendritic_delay );
  std::cout << std::setprecision(15) << "adj depression\t" << weight_ << "\t";
  weight_ = depress_( weight_, K_minus, cp );
  std::cout << std::setprecision(15) << "\t" << t_lastspike_ + cp.axonal_delay_ - minus_dt << "\t" << t_spike + cp.axonal_delay_ << "\t" << K_minus << "\t" << weight_ << std::endl;

  SpikeEvent e;
  e.set_receiver( *target );
  e.set_weight( weight_ - ori_weight_ );
  e.set_delay_steps( get_delay_steps() );
  e.set_rport( get_rport() );
  e.set_stamp( Time::ms_stamp( t_spike ) );
  e();
}

} // of namespace nest

#endif // of #ifndef STDP_PL_SYNAPSE_HOM_AX_DELAY_H
