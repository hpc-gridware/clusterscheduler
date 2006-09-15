/**
 * Copyright 2003-2004 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms.
 * -----------------------------------------------------------------------------
 *  Generated from java_event_factory.jsp
 *  !!! DO NOT EDIT THIS FILE !!!
 */
<%
  com.sun.grid.cull.JavaHelper jh = (com.sun.grid.cull.JavaHelper)params.get("javaHelper");
  com.sun.grid.cull.CullDefinition cullDef = (com.sun.grid.cull.CullDefinition)params.get("cullDef");
%>
package com.sun.grid.jgdi.event;


import java.util.*;

<% // Import all cull object names;
    java.util.Iterator iter = cullDef.getObjectNames().iterator();
    com.sun.grid.cull.CullObject cullObj = null;
    String name = null;
    
    

    while( iter.hasNext() ) {
      name = (String)iter.next();
      cullObj = cullDef.getCullObject(name); 
/*      if (cullObj.getType() == cullObj.TYPE_PRIMITIVE) {
         continue;
      }
      if(cullObj.getType() == cullObj.TYPE_MAPPED) {
         continue;
      }
      if(!cullObj.isRootObject()) {
        continue;
      }*/
%>import <%=jh.getFullClassName(cullObj)%>;
<% } // end of while %>

/**
 *  Factory class for all List/Del/Add/Mod Events of cull objects
 *  @author richard.hierlmeier@sun.com
 */
public class EventFactory extends EventFactoryBase {

   /** store all internal event factories (key the name of the cull object,
    *  value is the internal event factory) */
   private static Map facMap;
   
   
  /**
   * Create a list event for a cull type
   * @param name        name of the cull type
   * @param timestamp   timestamp when the list event occured
   * @param eventId     unique id of the event
   * @return the list event
   */
  public static ListEvent createListEvent(String name, long timestamp, int eventId) {
   
      InternalEventFactory fac = (InternalEventFactory)facMap.get(name);
      if( fac != null ) {
         return fac.createListEvent(timestamp, eventId);
      }
      throw new IllegalArgumentException( "can not create list event for cull type " + name );
  }
  
  /**
   * Create a add event for a cull type
   * @param name        name of the cull type
   * @param timestamp   timestamp when the add event occured
   * @param eventId     unique id of the event
   * @return the add event
   */
   public static AddEvent createAddEvent(String name, long timestamp, int eventId) {
   
      InternalEventFactory fac = (InternalEventFactory)facMap.get(name);
      if( fac != null ) {
         return fac.createAddEvent(timestamp, eventId);
      }
      throw new IllegalArgumentException( "can not create add event for cull type " + name );
   }

  /**
   * Create a mod event for a cull type
   * @param name        name of the cull type
   * @param timestamp   timestamp when the mod event occured
   * @param eventId     unique id of the event
   * @return the mod event
   */
   public static ModEvent createModEvent(String name, long timestamp, int eventId) {
   
      InternalEventFactory fac = (InternalEventFactory)facMap.get(name);
      if( fac != null ) {
         return fac.createModEvent(timestamp, eventId);
      }
      throw new IllegalArgumentException( "can not create mod event for cull type " + name );
   }
  
  /**
   * Create a del event for a cull type
   * @param name        name of the cull type
   * @param timestamp   timestamp when the del event occured
   * @param eventId     unique id of the event
   * @return the del event
   */
   public static DelEvent createDelEvent(String name, long timestamp, int eventId) {
   
      InternalEventFactory fac = (InternalEventFactory)facMap.get(name);
      if( fac != null ) {
         return fac.createDelEvent(timestamp, eventId);
      }
      throw new IllegalArgumentException( "can not create del event for cull type " + name );
   }

   /**
    *  Interface for all internal event factory classes
    */
   interface InternalEventFactory  {

       public ListEvent createListEvent(long timestamp, int eventId);
       public AddEvent  createAddEvent(long timestamp, int eventId);
       public ModEvent  createModEvent(long timestamp, int eventId);
       public DelEvent  createDelEvent(long timestamp, int eventId);    
    }

    /**
     *  this static initializer creates all internal event factories an stores
     *  it the the factory map.
     */
    static {        
    
        facMap = new HashMap();
        
<%
    iter = cullDef.getObjectNames().iterator();
    while( iter.hasNext() ) {
      name = (String)iter.next();
      cullObj = cullDef.getCullObject(name); 
       String idlName = cullObj.getIdlName();
        // <%=cullObj.getName()%>
        facMap.put("<%=cullObj.getName()%>", new <%=idlName%>EventFactory()); 
<%
    } // end of while
%>
    
    }
    // -------------------- Factory classes -----------------------------------
<%
    iter = cullDef.getObjectNames().iterator();
    while( iter.hasNext() ) {
      name = (String)iter.next();
      cullObj = cullDef.getCullObject(name); 
      String idlName = cullObj.getIdlName();
%>
     // <%=cullObj.getName()%>
    static class <%=idlName%>EventFactory implements InternalEventFactory {
    
       public ListEvent createListEvent(long timestamp, int eventId) {
<%
        if(cullObj.hasGetListOperation()) {
%>
           return new <%=idlName%>ListEvent(timestamp, eventId);
<%
        } else {
%>
           throw new IllegalStateException("list event not supported for <%=cullObj.getName()%>");
<%
        }
%>  
       }
       public AddEvent  createAddEvent(long timestamp, int eventId) {
<%
        if(cullObj.hasAddOperation()) {
%>
           return new <%=idlName%>AddEvent(timestamp, eventId);
<%
        } else {
%>
           throw new IllegalStateException("add event not supported for <%=cullObj.getName()%>");
<%
        }
%>  
       }
       public ModEvent  createModEvent(long timestamp, int eventId) {
<%
        if(cullObj.hasModifyOperation()) {
%>
           return new <%=idlName%>ModEvent(timestamp, eventId);
<%
        } else {
%>
           throw new IllegalStateException("mod event not supported for <%=cullObj.getName()%>");
<%
        }
%>  
       }
       public DelEvent  createDelEvent(long timestamp, int eventId) {
<%
        if(cullObj.hasDeleteOperation()) {
%>
           return new <%=idlName%>DelEvent(timestamp, eventId);
<%
        } else {
%>
           throw new IllegalStateException("del event not supported for <%=cullObj.getName()%>");
<%
        }
%>  
       }
    }
    
<% 
    } // end of while
%>

}
