#include <array>

#include "test.h"
#include "driver.h"
#include "channels.h"

namespace elevator {

static const int N_FLOORS = 4;
static const int N_BUTTONS = 3;

static const std::array< std::array< int, N_BUTTONS >, N_FLOORS > lampChannelMatrix{ {
    { { LIGHT_UP1, LIGHT_DOWN1, LIGHT_COMMAND1 } },
    { { LIGHT_UP2, LIGHT_DOWN2, LIGHT_COMMAND2 } },
    { { LIGHT_UP3, LIGHT_DOWN3, LIGHT_COMMAND3 } },
    { { LIGHT_UP4, LIGHT_DOWN4, LIGHT_COMMAND4 } },
} };

static const std::array< std::array< int, N_BUTTONS >, N_FLOORS > buttonChannelMatrix{ {
    { { FLOOR_UP1, FLOOR_DOWN1, FLOOR_COMMAND1 } },
    { { FLOOR_UP2, FLOOR_DOWN2, FLOOR_COMMAND2 } },
    { { FLOOR_UP3, FLOOR_DOWN3, FLOOR_COMMAND3 } },
    { { FLOOR_UP4, FLOOR_DOWN4, FLOOR_COMMAND4 } },
} };

int lamp( Button btn ) {
    assert_leq( btn.floor(), int( lampChannelMatrix.size() ), "out-of-bounds" );
    assert_leq( 1, btn.floor(), "out-of-bounds" );

    return lampChannelMatrix[ btn.floor() - 1 ][ int( btn.type() ) ];
}

int button( Button btn ) {
    assert_leq( btn.floor(), int( buttonChannelMatrix.size() ), "out-of-bounds" );
    assert_leq( 1, btn.floor(), "out-of-bounds" );

    return buttonChannelMatrix[ btn.floor() - 1 ][ int( btn.type() ) ];
}

Driver::Driver() : _minFloor( 1 ), _maxFloor( 4 ),
    // we don't know what to set yet, just plug in some sensible values
    _lastDirection( Direction::Down ), _lastFloor( 1 ), _moving( false )
{
    stopElevator(); // for safety reasons
    int sensor = getFloor();
    if ( sensor != -1 )
        _lastFloor = sensor;
}

/** be nicer to elevator
 * in case exception is thrown there is chance elevator is running, if
 * Driver goes out of scope we want elevator to stop for safety reasons
 */
Driver::~Driver() { stopElevator(); }

void Driver::init() {
    for ( int i = _minFloor; i <= _maxFloor; ++i ) {
        if ( i != _maxFloor )
            setButtonLamp( Button{ ButtonType::CallUp, i }, false );
        if ( i != _minFloor )
            setButtonLamp( Button{ ButtonType::CallDown, i }, false );
        setButtonLamp( Button{ ButtonType::TargetFloor, i }, false );
    }

    setStopLamp( false );
    setDoorOpenLamp( false );

    goToBottom();
}

void Driver::shutdown() {
    init(); // for now
}

void Driver::setButtonLamp( Button btn, bool state ) {
    _lio.io_set_bit( lamp( btn ), state );
}

void Driver::setStopLamp( bool state ) {
    _lio.io_set_bit( LIGHT_STOP, state );
}

void Driver::setDoorOpenLamp( bool state ) {
    _lio.io_set_bit( DOOR_OPEN, state );

}

void Driver::setFloorIndicator( int floor ) {
    assert_leq( _minFloor, floor, "floor out of bounds" );
    assert_leq( floor, _maxFloor, "floor out of bounds" );
    --floor; // floor numers are from 0 in backend but from 1 in driver and labels

    _lio.io_set_bit(FLOOR_IND1, (floor & 0x02) == 0x02 );
    _lio.io_set_bit(FLOOR_IND2, (floor & 0x01) == 0x01 );
}

bool Driver::getButtonLamp( Button button ) {
    return _lio.io_read_bit( lamp( button ) );
}

bool Driver::getStopLamp() {
    return _lio.io_read_bit( LIGHT_STOP );
}

bool Driver::getDoorOpenLamp() {
    return _lio.io_read_bit( DOOR_OPEN );
}

int Driver::getFloorIndicator() {
    return ((_lio.io_read_bit( FLOOR_IND1 ) << 1) | (_lio.io_read_bit( FLOOR_IND2 ))) + 1;
}

bool Driver::getButtonSignal( Button btn ) {
    return _lio.io_read_bit( button( btn ) );
}

void Driver::_setMotorSpeed( Direction direction, int speed ) {
    assert_lt( 0, speed, "Speed must be positive" );
    _lastDirection = direction;
    _lio.io_set_bit( MOTORDIR, direction == Direction::Down );
    _lio.io_write_analog( MOTOR, 2048 + 4 * speed );
};

void Driver::stopElevator() {
    _lio.io_set_bit( MOTORDIR, _lastDirection == Direction::Up ); // reverse direction
    _lio.io_write_analog( MOTOR, 0 ); // actually stop
    _moving = false;
};

int Driver::getFloor() {
    if ( _lio.io_read_bit( SENSOR1 ) )
        return 1;
    else if ( _lio.io_read_bit( SENSOR2 ) )
        return 2;
    else if ( _lio.io_read_bit( SENSOR3 ) )
        return 3;
    else if ( _lio.io_read_bit( SENSOR4 ) )
        return 4;
    else
        return -1;

};
bool Driver::getStop() { assert_unimplemented(); };
bool Driver::getObstruction() { assert_unimplemented(); };

void Driver::goToFloor( int floor ) {
    assert_leq( _minFloor, floor, "floor out of bounds" );
    assert_leq( floor, _maxFloor, "floor out of bounds" );
    assert( !_moving, "inconsistent state" );
    if ( floor == _lastFloor )
        return;
    if ( floor < _lastFloor )
        goDownToFloor( floor );
    else
        goUpToFloor( floor );
}

void Driver::goUpToFloor( int floor ) {
    _goTo( Direction::Up, floor );
}

void Driver::goDownToFloor( int floor ) {
    _goTo( Direction::Down, floor );
}

void Driver::goToBottom() { goDownToFloor( _minFloor ); }
void Driver::goToTop() { goUpToFloor( _maxFloor ); }

void Driver::_goTo( Direction dir, int targetFloor ) {
    _setMotorSpeed( dir, 300 );
    int on = -1;
    while ( true ) {
        on = getFloor();
        _movingOnFloor( on );
        if ( on == targetFloor )
            break;
        if ( dir == Direction::Up && on == _maxFloor )
            break;
        if ( dir == Direction::Down && on == _minFloor )
            break;
    }
    stopElevator();
    setFloorIndicator( targetFloor );
}

void Driver::_movingOnFloor( int floorSensor ) {
    if ( floorSensor != -1 ) {
        _lastFloor = floorSensor;
        setFloorIndicator( floorSensor );
    }
    _moving = true;
}

}
