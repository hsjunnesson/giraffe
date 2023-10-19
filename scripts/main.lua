function on_enter(engine, game)
end

function on_leave(engine, game)
end

function on_input(engine, game, input_command)
    if input_command.input_type == Engine.InputType.Key then
        local pressed = input_command.key_state.trigger_state == Engine.TriggerState.Pressed
        local repeated = input_command.key_state.trigger_state == Engine.TriggerState.Repeated

        local bind_action_key = Engine.action_key_for_input_command(input_command)

        if not bind_action_key:valid() then
            return
        end

        local action_hash = Hash.get_identifier(game.action_binds.bind_actions, bind_action_key, Game.ActionHash.NONE)

        if action_hash == Game.ActionHash.NONE then
            return
        elseif action_hash == Game.ActionHash.QUIT then
           if pressed then
               Game.transition(engine, game, Game.AppState.Quitting)
           end
        end
    end
end

function update()
end

function render()
end

function render_imgui()
end
