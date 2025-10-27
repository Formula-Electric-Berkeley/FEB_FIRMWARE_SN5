#ifndef __TMUX1208_H
    #define __TMUX1208_H
    
    #include "FEB_Main.h"
    
    /**
    * @brief Start the TMUX1208 Component.
    * 
    * This function starts the TMUX1208 component, 
    * enabling it and setting the channel 0.
    */
    void TMUX1208_Start(void);
    
    /**
    * @brief Enable the TMUX1208 Component.
    * 
    * This function enables the TMUX1208 component.
    */
    void TMUX1208_Enable(void);
    
    /**
    * @brief Disable the TMUX1208 Component.
    * 
    * This function disables the TMUX1208 component.
    */
    void TMUX1208_Disable(void);
    
    /**
    * @brief Resets the TMUX1208 Component.
    * 
    * This function resets the TMUX1208 component 
    * by setting channel 0.
    */
    void TMUX1208_Reset(void);
    
    /**
    * @brief Sets the active channel the TMUX1208 Component.
    * 
    * This function sets the active channel of the TMUX1208
    * by changing the appropriate values of the address pins.
    * @param channel the channel to be set.
    */
    void TMUX1208_SetChannel(uint8_t channel);
    
    /**
    * @brief Gets the current active channel of the TMUX1208.
    * 
    * This function gets the current active channel of the TMUX1208
    * by reading the values currently written to the address
    * pins.
    * @retval channel the current active channel.
    */
    uint8 TMUX1208_GetChannel(void);

#endif

/* [] END OF FILE */
