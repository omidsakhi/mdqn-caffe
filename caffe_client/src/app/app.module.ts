import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { HttpModule } from '@angular/http';
import { WorldComponent } from './world/world.component'
import { AppComponent } from './app.component';
import { DataService } from './data.service';
@NgModule({
  declarations: [
    AppComponent,
    WorldComponent    
  ],
  imports: [
    BrowserModule,
    FormsModule,
    HttpModule
  ],
  providers: [DataService],
  bootstrap: [AppComponent]
})
export class AppModule { }
