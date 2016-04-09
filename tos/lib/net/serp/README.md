SERP: Saint-Exupery Routing Protocol
==============================

```
Il semble que la perfection soit atteinte non quand il n'y a plus rien à
ajouter, mais quand il n'y a plus rien à retrancher.

It seems that perfection is attained not when there is nothing more to add, but
when there is nothing more to remove
```

## Implementation Plan

When there's time...
* [ ] Disable the usual router solicitation stuff
* [ ] Broadcast Router Solicitation messages on a trickle timer
* ~~[ ] Implement the custom RS option containing the right info~~
  I'm pretty sure we don't have custom RS options?
* [x] Make sure we can trigger on a received RS
* [ ] Generate an RA in response to RS that has our payload info
  This payload info is:
    * our prefix
    * our hop count
    * whether or not we are powered

Here we make notes on implementation details


## Trickle Timer

How do we define/use trickle timers?

```nesc
uses {
    interface Timer<TMilli> as TrickleTimer;
}

uint32_t tricklePeriod = 0;
bool fired = FALSE;

void resetTrickleTimer() {
    call TrickleTimer.stop();
    tricklePeriod = 2 << (intervalMin - 1);
    redundancyCounter = 0;
    doubleCounter = 0;
}

void chooseTrickleTime() {
    call TrickleTimer.stop();
    randomTime = tricklePeriod;
    randomTime /= 2;
    randomTime += call Random.rand32() % randomTime;
    call TrickleTimer.startOneShot(randomTime);
}

void computeTrickleRemaining() {
    uint32_t remain = tricklePeriod - randomTime;
    fired = TRUE;
    call TrickleTimer.startOneShot(remain);
}

void nextTrickleTime() {
    fired = FALSE;
    if (doubleCounter < intervalMin) {
        doubleCounter++;
        tricklePeriod *= 2;
    }
    if (!call TrickleTimer.isRunning()) {
        chooseTrickleTime();
    }
}

event void TrickleTimer.fired() {
    if (fired) {
        nextTrickleTime();
    } else {
        // send packet here
        post computeTrickleRemaining();
    }
}
```
