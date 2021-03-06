import * as d3 from 'd3-selection'
import * as d3Scale from 'd3-scale'
import { Component, ElementRef, ViewChild, OnInit, AfterViewInit } from '@angular/core';
import { Vec } from '../vec'
import { Segment, IIntersectionResult } from '../segment'
import { Food, FoodType } from '../food'
import { Agent, AgentState } from '../agent'
import { Eye, SensedType } from '../eye'
import { DataService } from '../data.service'

@Component({
  selector: 'world',
  templateUrl: './world.component.html',
  styleUrls: ['./world.component.css']
})
export class WorldComponent implements AfterViewInit {
  public initialized: boolean = false;

  public eyeChartWidth: number = 320;
  public eyeChartHeight: number = 200;
  public eyeChartSvg1: any;
  public eyeChartSvg2: any;
  public eyeChartX: any;
  public eyeChartY: any;

  public tickIntervalFreq: number = 60;
  public height: number;
  public width: number;
  public context: CanvasRenderingContext2D;
  public canvas: HTMLCanvasElement;
  public walls: Segment[] = [];
  public agents: Agent[] = [];
  public foods: Food[] = [];
  public miceImage: any;

  public textColor : string = "#333";
  public wallColor : string = "#1047A9";
  public agentColor : string[] = ["#E366B5","#C7007D"];
  public foodColor : string[] = ["#CFF76F","#A8F000"];
  public poisonColor : string[] = ["#FFCE73","#FFA500"];

  @ViewChild("worldCanvas") worldCanvas: ElementRef;
  constructor(private dataService: DataService) { }

  createBox(x: number, y: number, w: number, h: number, sides: number = 15 /* ALL */) {
    if (sides & 1) this.walls.push(new Segment(new Vec(x, y), new Vec(x + w, y)));
    if (sides & 2) this.walls.push(new Segment(new Vec(x + w, y), new Vec(x + w, y + h)));
    if (sides & 4) this.walls.push(new Segment(new Vec(x + w, y + h), new Vec(x, y + h)));
    if (sides & 8) this.walls.push(new Segment(new Vec(x, y + h), new Vec(x, y)));
  }

  createWalls() {
    var pad = 10;
    this.createBox(pad, pad, this.width - pad * 2, this.height - pad * 2);
    this.createBox(100, 100, 200, 300, 1 + 2 + 4);
    this.createBox(400, 100, 200, 300, 1 + 2 + 8);
  }

  recreateFood(num: number) {
    for (var k = this.foods.length; k < num; k++)
      this.foods.push(
        new Food(
          new Vec(
            this.randf(20, this.width - 20),
            this.randf(20, this.height - 20))
          , 10,
          this.randi(0, 2)
        )
      );
  }
  createAgents() {
    this.agents.push(new Agent("A009", new Vec(30, 30), 10, 0, 0.9));
    this.agents.push(new Agent("A005", new Vec(this.width - 30, this.height - 30), 10, 0, 0.5));
    this.agents.push(new Agent("A003", new Vec(this.width - 30, 30), 10, 0, 0.3));
    this.agents.push(new Agent("A001", new Vec(30, this.height - 30), 10, 0, 0.1));
    for (let a of this.agents) {
      this.processEyes(a);
      this.dataService.registerAgent(a);
    }
  }
  ngAfterViewInit() {
    this.canvas = this.worldCanvas.nativeElement;
    this.height = this.canvas.clientHeight;
    this.width = this.canvas.clientWidth;
    this.context = this.canvas.getContext("2d");
    this.miceImage = document.getElementById("mice-image");

    this.dataService.socket.on('server-data', (data) => {
      if (data.type == "action") {
        this.receivedAction(data.agent_id, data.action_index);
      }
    })
    this.dataService.socket.on('connect', () => {
      if (this.initialized == false) {
        this.initialized = true;
        this.createWalls();
        this.recreateFood(30);
        this.createAgents();
        this.initializeEyeCharts();
        this.draw();
        this.restart();
      }
      else {
        for (let a of this.agents)
          a.state = AgentState.Ready;
      }
    })
  }

  randi(min, max): number {
    return Math.floor(Math.random() * (max - min) + min);
  }
  randf(min, max): number {
    return Math.random() * (max - min) + min;
  }

  restart() {
    setTimeout(() => { this.tick() }, 1000.0 / this.tickIntervalFreq);
  }
  processEyes(a: Agent) {
    for (let e of a.eyes) {
      var res = this.collisionTest(a.position, a.eyeEndPoint(e), true, true);
      e.sensedProximity = (res != null) ? res.up.distance(a.position) : e.maxRange;
      e.sensedType = (res != null) ? res.type : SensedType.Nothing;
    }
  }
  processAgent(a: Agent, actionIndex: number) {
    var oldPosition = a.position;
    a.applyAction(actionIndex);
    var wallCollision = this.collisionTest(oldPosition, a.position, true, false);
    if (wallCollision != null)
      a.position = oldPosition;
    if (a.position.x < 0) a.position.x = 0;
    if (a.position.x > this.width) a.position.x = this.width;
    if (a.position.y < 0) a.position.y = 0;
    if (a.position.y > this.height) a.position.y = this.height;
    for (let f of this.foods) {
      var d = a.position.distance(f.position);
      if (d < (f.radius + a.radius)) {
        var wallCollision = this.collisionTest(a.position, f.position, true, false);
        if (wallCollision == null) {
          a.eatFood(f);
          this.foods.splice(this.foods.indexOf(f), 1);
          break;
        }
      }
    }
    var reward = a.reward();
    this.processEyes(a);

    if (a.epsilon > 0.15)
      this.dataService.sendTransition(a, a.oldInputs, actionIndex, reward, a.getInputs());

    a.state = AgentState.Ready;
  }
  findAgent(id: string): Agent {
    for (let a of this.agents)
      if (a.id == id) return a;
    return null;
  }
  receivedAction(agentId: string, action: number) {
    var agent = this.findAgent(agentId);
    if (agent == null) return; // should never happen
    this.processAgent(agent, action);
    this.recreateFood(30);
    this.draw();
  }
  tick() {
    for (let a of this.agents)
      if (a.state == AgentState.Ready) {
        a.state = AgentState.Waiting;
        a.oldInputs = a.getInputs();
        this.dataService.sendInputs(a, a.oldInputs);
      }
    this.restart();
  }
  collisionTest(p1: Vec, p2: Vec, checkWalls: boolean, checkFood: boolean): IIntersectionResult {
    var minres = null;
    var segment = new Segment(p1, p2);
    if (checkWalls == true) {
      for (var i = 0; i < this.walls.length; i++) {
        var wall = this.walls[i];
        var res = segment.segmentIntersect(wall);
        if (res !== null) {
          res.type = SensedType.Wall;
          if (minres == null)
            minres = res;
          else if (res.ua < minres.ua)
            minres = res;
        }
      }
    }

    if (checkFood == true) {
      for (var i = 0; i < this.foods.length; i++) {
        var food = this.foods[i];
        var res = segment.circleIntersect(food.position, food.radius);
        if (res !== null) {
          res.type = (food.type == FoodType.Bad) ? SensedType.BadFood : SensedType.GoodFood;
          if (minres == null)
            minres = res;
          else if (res.ua < minres.ua)
            minres = res;
        }
      }
    }

    return minres;
  }
  drawWalls() {
    this.context.lineWidth = 1;
    this.context.strokeStyle = this.wallColor;
    this.context.beginPath();
    for (let w of this.walls) {
      this.context.moveTo(w.p1.x, w.p1.y);
      this.context.lineTo(w.p2.x, w.p2.y);
    }
    this.context.stroke();
  }
  drawFood() {
    this.context.lineWidth = 2;    
    for (let f of this.foods) {
      if (f.type == FoodType.Bad)
      {
          this.context.fillStyle = this.poisonColor[0];
          this.context.strokeStyle = this.poisonColor[1];
      }      
      else
      {
          this.context.fillStyle = this.foodColor[0];
          this.context.strokeStyle = this.foodColor[1];        
      }
      this.context.beginPath();
      this.context.arc(f.position.x, f.position.y, f.radius, 0, Math.PI * 2, true);
      this.context.fill();
      this.context.stroke();
    }
  }
  drawAgents() {

    for (let a of this.agents) {
      this.context.lineWidth = 1;
      this.context.fillStyle = this.textColor;

      for (let e of a.eyes) {
        if (e.sensedType === SensedType.Nothing) {
          this.context.setLineDash([5, 15]);
          this.context.strokeStyle = this.textColor;
        } else if (e.sensedType === SensedType.Wall) {
          this.context.setLineDash([5, 10]);
          this.context.strokeStyle = this.wallColor;
        } else if (e.sensedType === SensedType.GoodFood) {
          this.context.setLineDash([]);
          this.context.strokeStyle = this.foodColor[1];
        } else if (e.sensedType === SensedType.BadFood) {
          this.context.setLineDash([]);
          this.context.strokeStyle = this.poisonColor[1];
        }
        this.context.beginPath();
        this.context.moveTo(a.position.x, a.position.y);
        this.context.lineTo(a.position.x + e.sensedProximity * Math.sin(a.angle + e.angle), a.position.y + e.sensedProximity * Math.cos(a.angle + e.angle));
        this.context.stroke();
      }

      this.context.setLineDash([]);
      this.context.fillStyle = this.agentColor[0];
      this.context.strokeStyle = this.agentColor[1];
      this.context.lineWidth = 4;
      this.context.beginPath();
      this.context.arc(a.position.x, a.position.y, a.radius, 0, Math.PI * 2, true);
      this.context.fill();
      this.context.stroke();
      
      this.context.font = "8px Arial";
      this.context.fillText(a.name, a.position.x + a.radius * 2 + 3, a.position.y - 15);
      this.context.fillText(a.lastReward.toFixed(2), a.position.x + a.radius * 2 + 3, a.position.y - 5);
      this.context.fillText(a.totalGoodFood.toString(), a.position.x + a.radius * 2 + 3, a.position.y + 5);
      this.context.fillText(a.totalBadFood.toString(), a.position.x + a.radius * 2 + 3, a.position.y + 15);

    }

  }
  draw() {
    this.context.clearRect(0, 0, this.width, this.height);
    this.drawWalls();
    this.drawFood();
    this.drawAgents();
    this.eyeChart();
  }
  backupNetwork() {
    this.dataService.backupNetwork();
  }
  initializeEyeCharts() {
    this.eyeChartSvg1 = d3.select("#first-agent-eyes");
    this.eyeChartSvg2 = d3.select("#second-agent-eyes");
    this.eyeChartX = d3Scale.scaleLinear()
      .range([0, this.eyeChartWidth])
      .domain([0, 27]);
    this.eyeChartY = d3Scale.scaleLinear()
      .range([this.eyeChartHeight, 0])
      .domain([0.0, 1.0]);
  }
  eyeChart() {
    this.eyeChartSvg1.selectAll(".bar")
      .data(this.agents[0].oldInputs)
      .enter().append("rect")
      .attr("class", "bar")
      .attr("x", (d, i) => { return this.eyeChartX(i); })
      .attr("y", 0)
      .attr("width", this.eyeChartWidth / 27)
      .attr("height", d => { return this.eyeChartHeight - this.eyeChartY(d); })
      .style("fill", (d, i) => {
        if (i % 3 == 0)
          return this.wallColor;
        else if (i % 3 == 1)
          return this.foodColor[0];
        else
          return this.poisonColor[0];
      })
      .style("stroke", this.textColor);

    this.eyeChartSvg1.selectAll(".bar")
      .attr("height", d => { return this.eyeChartHeight - this.eyeChartY(d); });

    this.eyeChartSvg2.selectAll(".bar")
      .data(this.agents[1].oldInputs)
      .enter().append("rect")
      .attr("class", "bar")
      .attr("x", (d, i) => { return this.eyeChartX(i); })
      .attr("y", 0)
      .attr("width", this.eyeChartWidth / 27)
      .attr("height", d => { return this.eyeChartHeight - this.eyeChartY(d); })
      .style("fill", (d, i) => {
        if (i % 3 == 0)
          return this.wallColor;
        else if (i % 3 == 1)
          return this.foodColor[0];
        else
          return this.poisonColor[0];
      })
      .style("stroke", this.textColor);

    this.eyeChartSvg2.selectAll(".bar")
      .attr("height", d => { return this.eyeChartHeight - this.eyeChartY(d); });

  }
}
