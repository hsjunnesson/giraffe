function on_enter(engine, game)
end

function on_leave(engine, game)
end

function on_input(engine, game, input_command)
    if input_command.input_type == Engine.InputType.Key then
        local pressed = input_command.key_state.trigger_state == Engine.TriggerState.Pressed
        local repeated = input_command.key_state.trigger_state == Engine.TriggerState.Repeated

        local bind_action_key = Engine.action_key_for_input_command(input_command)

        if not valid_identifier(bind_action_key) then
            return
        end

        print(Game.ActionHash.QUIT == Game.ActionHash.NONE)
        print(Game.ActionHash.QUIT == Game.ActionHash.QUIT)

        -- if bind_action_key == 0 then
        --     return
        -- end

        -- print("Bind action key: " .. bind_action_key)
    end
end

function update()
end

function render()
end

function render_imgui()
end
