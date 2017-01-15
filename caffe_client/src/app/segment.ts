import { Vec } from './vec'

export interface IIntersectionResult {
    ua : number,
    ub? : number,
    up : Vec,
    type? : number
}

export class Segment {
    constructor(
        public p1: Vec,
        public p2: Vec
    ) { }

    segmentIntersect(s: Segment) : IIntersectionResult {
        var denom = (s.p2.y - s.p1.y) * (this.p2.x - this.p1.x) - (s.p2.x - s.p1.x) * (this.p2.y - this.p1.y);
        if (denom === 0.0) return null; // parallel lines
        var ua = ((s.p2.x - s.p1.x) * (this.p1.y - s.p1.y) - (s.p2.y - s.p1.y) * (this.p1.x - s.p1.x)) / denom;
        var ub = ((this.p2.x - this.p1.x) * (this.p1.y - s.p1.y) - (this.p2.y - this.p1.y) * (this.p1.x - s.p1.x)) / denom;
        if (ua > 0.0 && ua < 1.0 && ub > 0.0 && ub < 1.0)            
            return { 
                ua: ua, 
                ub: ub, 
                up: new Vec(this.p1.x + ua * (this.p2.x - this.p1.x), this.p1.y + ua * (this.p2.y - this.p1.y))                
            };    
        return null;
    }

    circleIntersect(center: Vec, radius: number) : IIntersectionResult {
        var perpendicular = new Vec(this.p2.y - this.p1.y, -(this.p2.x - this.p1.x));
        var distance = Math.abs((this.p2.x - this.p1.x) * (this.p1.y - center.y) - (this.p1.x - center.x) * (this.p2.y - this.p1.y));
        distance = distance / perpendicular.length();
        if (distance > radius) return null;
        var up = center.add(perpendicular.normalize().scale(distance));
        if (Math.abs(this.p2.x - this.p1.x) > Math.abs(this.p2.y - this.p1.y))
            var ua = (up.x - this.p1.x) / (this.p2.x - this.p1.x);
        else
            var ua = (up.y - this.p1.y) / (this.p2.y - this.p1.y);
        if (ua > 0.0 && ua < 1.0)
            return { 
                ua: ua, 
                up: up 
            };
        return null;
    }
}
